#include "task.h"

#define LORA_FRAME_HEAD 0xAAU
#define LORA_FRAME_TAIL 0x55U
#define LORA_FRAME_TYPE_TEMP 0x01U
#define LORA_FRAME_TYPE_LIGHT 0x02U
#define LORA_FRAME_SIZE 6U

#define STS30_ADDR 0x94U
#define STS30_CMD_MEASURE_HIGH_REPEAT_MSB 0x24U
#define STS30_CMD_MEASURE_HIGH_REPEAT_LSB 0x0BU

#define LIGHT_ADC_GPIO_PORT GPIOB
#define LIGHT_ADC_GPIO_PIN GPIO_PIN_3
#define LIGHT_ADC_CHANNEL ADC_CHANNEL_2

#if APP_NODE_ROLE == APP_NODE_LIGHT
#define SENSOR_SEND_OFFSET_MS 500U
#else
#define SENSOR_SEND_OFFSET_MS 0U
#endif

Interface State = STATE_MAIN;
Interface Old_State = STATE_MAIN;
Task_Time_Adj Task_Time;
KEY_State KEY;

uint8_t LoRa_TX_Buf[LORA_FRAME_SIZE];
uint8_t LoRa_RX_Buf[LORA_FRAME_SIZE];
uint16_t Time_Num;
uint8_t Key_Flag;

static int16_t App_Local_Temp_X10;
static int16_t App_Remote_Temp_X10;
static uint16_t App_Local_Light_Raw;
static uint16_t App_Remote_Light_Raw;
static uint8_t App_Local_Valid;
static uint8_t App_Remote_Valid;
static uint32_t App_Last_Tx_Tick;
static uint32_t App_Last_Rx_Tick;

#if APP_NODE_ROLE == APP_NODE_TEMP
I2C_HandleTypeDef hi2c2;
static void STS30_Init(void);
static HAL_StatusTypeDef STS30_StartConversion(void);
static HAL_StatusTypeDef STS30_ReadTempX10(int16_t *temp_x10);
#elif APP_NODE_ROLE == APP_NODE_LIGHT
static void LightSensor_Init(void);
static HAL_StatusTypeDef LightSensor_ReadRaw(uint16_t *light_raw);
#else
#error "APP_NODE_ROLE must be APP_NODE_TEMP or APP_NODE_LIGHT"
#endif

static uint8_t FrameChecksum(uint8_t type, uint16_t value);
static void BuildFrame(uint8_t *buf, uint8_t type, uint16_t value);
static uint8_t ParseFrame(const uint8_t *buf, uint8_t size, uint8_t *type, uint16_t *value);
static void SendLocalFrame(void);
static void ReceiveRemoteFrame(void);
static void FormatTempString(uint8_t *buf, const char *prefix, int16_t temp_x10);
static void FormatLightString(uint8_t *buf, const char *prefix, uint16_t light_raw);

void BSP_Init(void)
{
    OLED_Init();
    BSP_RADIO_Init();
    LORA_Init();

#if APP_NODE_ROLE == APP_NODE_TEMP
    STS30_Init();
    State = STATE_TEMP_NODE;
#else
    LightSensor_Init();
    State = STATE_LIGHT_NODE;
#endif

    Old_State = STATE_MAIN;
    App_Last_Rx_Tick = HAL_GetTick();
    App_Last_Tx_Tick = HAL_GetTick() - SENSOR_SEND_PERIOD_MS + SENSOR_SEND_OFFSET_MS;
}

void OLED_Proc(void)
{
    uint8_t oled_buf_Line1[20] = {0};
    uint8_t oled_buf_Line2[20] = {0};

    if(Task_Time.Oled_Time < OLED_TIME) return;
    Task_Time.Oled_Time = 0;

    if(Old_State != State)
    {
        Old_State = State;
        OLED_Clear();
    }

#if APP_NODE_ROLE == APP_NODE_TEMP
    if(App_Local_Valid) FormatTempString(oled_buf_Line1, "TX ", App_Local_Temp_X10);
    else sprintf((char *)oled_buf_Line1, "TX T:--.-C");

    if(App_Remote_Valid && ((HAL_GetTick() - App_Last_Rx_Tick) <= SENSOR_RX_TIMEOUT_MS))
    {
        FormatLightString(oled_buf_Line2, "RX ", App_Remote_Light_Raw);
    }
    else
    {
        App_Remote_Valid = 0;
        sprintf((char *)oled_buf_Line2, "RX L:----");
    }
#else
    if(App_Local_Valid) FormatLightString(oled_buf_Line1, "TX ", App_Local_Light_Raw);
    else sprintf((char *)oled_buf_Line1, "TX L:----");

    if(App_Remote_Valid && ((HAL_GetTick() - App_Last_Rx_Tick) <= SENSOR_RX_TIMEOUT_MS))
    {
        FormatTempString(oled_buf_Line2, "RX ", App_Remote_Temp_X10);
    }
    else
    {
        App_Remote_Valid = 0;
        sprintf((char *)oled_buf_Line2, "RX T:--.-C");
    }
#endif

    OLED_ShowString(0, 0, oled_buf_Line1, 16);
    OLED_ShowString(0, 2, oled_buf_Line2, 16);
}

void KEY_Proc(void)
{
    uint8_t key_val;

    if(Task_Time.Key_Time < KEY_TIME) return;
    Task_Time.Key_Time = 0;

    if(HAL_GPIO_ReadPin(ASW1_GPIO_Port, ASW1_Pin) == GPIO_PIN_RESET) key_val = ASW1;
    else if(HAL_GPIO_ReadPin(ASW2_GPIO_Port, ASW2_Pin) == GPIO_PIN_RESET) key_val = ASW2;
    else key_val = ASW_None;

    KEY.KEY_down = key_val & (KEY.KEY_old ^ key_val);
    KEY.KEY_up = ~key_val & (KEY.KEY_old ^ key_val);
    KEY.KEY_old = key_val;

    if(KEY.KEY_down == ASW2)
    {
        SendLocalFrame();
        App_Last_Tx_Tick = HAL_GetTick();
    }
}

void LoRa_Proc(void)
{
    uint32_t now = HAL_GetTick();

    ReceiveRemoteFrame();

    if((now - App_Last_Tx_Tick) < SENSOR_SEND_PERIOD_MS) return;
    App_Last_Tx_Tick = now;
    SendLocalFrame();
}

#if APP_NODE_ROLE == APP_NODE_TEMP
static void STS30_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};
    RCC_PeriphCLKInitTypeDef PeriphClkInitStruct = {0};

    PeriphClkInitStruct.PeriphClockSelection = RCC_PERIPHCLK_I2C2;
    PeriphClkInitStruct.I2c2ClockSelection = RCC_I2C2CLKSOURCE_PCLK1;
    if(HAL_RCCEx_PeriphCLKConfig(&PeriphClkInitStruct) != HAL_OK)
    {
        Error_Handler();
    }

    __HAL_RCC_GPIOA_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_11 | GPIO_PIN_12;
    GPIO_InitStruct.Mode = GPIO_MODE_AF_OD;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    GPIO_InitStruct.Alternate = GPIO_AF4_I2C2;
    HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

    __HAL_RCC_I2C2_CLK_ENABLE();

    hi2c2.Instance = I2C2;
    hi2c2.Init.Timing = 0x0090194B;
    hi2c2.Init.OwnAddress1 = 0;
    hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
    hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
    hi2c2.Init.OwnAddress2 = 0;
    hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
    hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
    hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
    if(HAL_I2C_Init(&hi2c2) != HAL_OK)
    {
        Error_Handler();
    }
    if(HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
    {
        Error_Handler();
    }
    if(HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) != HAL_OK)
    {
        Error_Handler();
    }

    STS30_StartConversion();
}

static HAL_StatusTypeDef STS30_StartConversion(void)
{
    uint8_t cmd[2] = {STS30_CMD_MEASURE_HIGH_REPEAT_MSB, STS30_CMD_MEASURE_HIGH_REPEAT_LSB};

    return HAL_I2C_Master_Transmit(&hi2c2, STS30_ADDR, cmd, 2, 10);
}

static HAL_StatusTypeDef STS30_ReadTempX10(int16_t *temp_x10)
{
    uint8_t data[2] = {0};
    HAL_StatusTypeDef status;
    uint16_t raw;
    int32_t temp;

    if(temp_x10 == 0) return HAL_ERROR;

    status = HAL_I2C_Master_Receive(&hi2c2, STS30_ADDR, data, 2, 10);
    if(status != HAL_OK)
    {
        STS30_StartConversion();
        return status;
    }

    raw = ((uint16_t)data[0] << 8) | data[1];
    temp = (((int32_t)raw * 1750L + 32767L) / 65535L) - 450L;

    status = STS30_StartConversion();
    if(status != HAL_OK) return status;

    *temp_x10 = (int16_t)temp;
    return HAL_OK;
}
#else
static void LightSensor_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin = LIGHT_ADC_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(LIGHT_ADC_GPIO_PORT, &GPIO_InitStruct);
}

static HAL_StatusTypeDef LightSensor_ReadRaw(uint16_t *light_raw)
{
    ADC_ChannelConfTypeDef sConfig = {0};

    if(light_raw == 0) return HAL_ERROR;

    sConfig.Channel = LIGHT_ADC_CHANNEL;
    sConfig.Rank = ADC_REGULAR_RANK_1;
    sConfig.SamplingTime = ADC_SAMPLINGTIME_COMMON_1;
    if(HAL_ADC_ConfigChannel(&hadc, &sConfig) != HAL_OK)
    {
        return HAL_ERROR;
    }

    if(HAL_ADC_Start(&hadc) != HAL_OK)
    {
        return HAL_ERROR;
    }
    if(HAL_ADC_PollForConversion(&hadc, 20) != HAL_OK)
    {
        HAL_ADC_Stop(&hadc);
        return HAL_TIMEOUT;
    }

    *light_raw = (uint16_t)HAL_ADC_GetValue(&hadc);
    HAL_ADC_Stop(&hadc);
    return HAL_OK;
}
#endif

static uint8_t FrameChecksum(uint8_t type, uint16_t value)
{
    uint8_t value_hi = (uint8_t)((value >> 8) & 0xFFU);
    uint8_t value_lo = (uint8_t)(value & 0xFFU);

    return (uint8_t)(type ^ value_hi ^ value_lo);
}

static void BuildFrame(uint8_t *buf, uint8_t type, uint16_t value)
{
    buf[0] = LORA_FRAME_HEAD;
    buf[1] = type;
    buf[2] = (uint8_t)((value >> 8) & 0xFFU);
    buf[3] = (uint8_t)(value & 0xFFU);
    buf[4] = FrameChecksum(type, value);
    buf[5] = LORA_FRAME_TAIL;
}

static uint8_t ParseFrame(const uint8_t *buf, uint8_t size, uint8_t *type, uint16_t *value)
{
    uint16_t parsed_value;

    if(buf == 0 || type == 0 || value == 0) return 0;
    if(size != LORA_FRAME_SIZE) return 0;
    if(buf[0] != LORA_FRAME_HEAD || buf[5] != LORA_FRAME_TAIL) return 0;
    if(buf[1] != LORA_FRAME_TYPE_TEMP && buf[1] != LORA_FRAME_TYPE_LIGHT) return 0;

    parsed_value = (uint16_t)(((uint16_t)buf[2] << 8) | buf[3]);
    if(buf[4] != FrameChecksum(buf[1], parsed_value)) return 0;

    *type = buf[1];
    *value = parsed_value;
    return 1;
}

static void SendLocalFrame(void)
{
#if APP_NODE_ROLE == APP_NODE_TEMP
    int16_t temp_x10;

    if(STS30_ReadTempX10(&temp_x10) != HAL_OK)
    {
        App_Local_Valid = 0;
        return;
    }

    App_Local_Temp_X10 = temp_x10;
    App_Local_Valid = 1;
    BuildFrame(LoRa_TX_Buf, LORA_FRAME_TYPE_TEMP, (uint16_t)temp_x10);
#else
    uint16_t light_raw;

    if(LightSensor_ReadRaw(&light_raw) != HAL_OK)
    {
        App_Local_Valid = 0;
        return;
    }

    App_Local_Light_Raw = light_raw;
    App_Local_Valid = 1;
    BuildFrame(LoRa_TX_Buf, LORA_FRAME_TYPE_LIGHT, light_raw);
#endif

    HAL_GPIO_WritePin(GPIOB, AL1_Pin, GPIO_PIN_RESET);
    LORA_Tx(LoRa_TX_Buf, LORA_FRAME_SIZE);
    HAL_GPIO_WritePin(GPIOB, AL1_Pin, GPIO_PIN_SET);
}

static void ReceiveRemoteFrame(void)
{
    uint8_t lora_size = 0;
    uint8_t type = 0;
    uint16_t value = 0;

    if(LORA_Rx(LoRa_RX_Buf, &lora_size) == 0) return;
    if(ParseFrame(LoRa_RX_Buf, lora_size, &type, &value) == 0) return;

#if APP_NODE_ROLE == APP_NODE_TEMP
    if(type != LORA_FRAME_TYPE_LIGHT) return;
    App_Remote_Light_Raw = value;
#else
    if(type != LORA_FRAME_TYPE_TEMP) return;
    App_Remote_Temp_X10 = (int16_t)value;
#endif

    App_Remote_Valid = 1;
    App_Last_Rx_Tick = HAL_GetTick();
    HAL_GPIO_WritePin(GPIOB, AL2_Pin, GPIO_PIN_RESET);
    HAL_Delay(5);
    HAL_GPIO_WritePin(GPIOB, AL2_Pin, GPIO_PIN_SET);
}

static void FormatTempString(uint8_t *buf, const char *prefix, int16_t temp_x10)
{
    int16_t abs_temp = temp_x10;

    if(abs_temp < 0) abs_temp = (int16_t)-abs_temp;

    if(temp_x10 < 0)
    {
        sprintf((char *)buf, "%sT:-%d.%dC", prefix, abs_temp / 10, abs_temp % 10);
    }
    else
    {
        sprintf((char *)buf, "%sT:%d.%dC", prefix, abs_temp / 10, abs_temp % 10);
    }
}

static void FormatLightString(uint8_t *buf, const char *prefix, uint16_t light_raw)
{
    sprintf((char *)buf, "%sL:%4u", prefix, (unsigned int)light_raw);
}
