#include "task.h"

#define LORA_TEMP_FRAME_HEAD 0xAAU
#define LORA_TEMP_FRAME_TAIL 0x55U
#define LORA_TEMP_FRAME_TYPE 0x01U
#define LORA_TEMP_FRAME_SIZE 6U
#define STS30_ADDR 0x94U
#define STS30_CMD_MEASURE_HIGH_REPEAT_MSB 0x24U
#define STS30_CMD_MEASURE_HIGH_REPEAT_LSB 0x0BU

Interface State = STATE_MAIN;
Interface Old_State = STATE_MAIN;
Task_Time_Adj Task_Time;
KEY_State KEY;

#if APP_ROLE_TX_SENSOR
uint8_t LoRa_TX_Buf[LORA_TEMP_FRAME_SIZE];
#else
uint8_t LoRa_RX_Buf[LORA_TEMP_FRAME_SIZE];
#endif
uint16_t Time_Num;
uint8_t Key_Flag;

static int16_t App_Temp_X10;
static uint8_t App_Temp_Valid;
#if APP_ROLE_TX_SENSOR
static uint32_t App_Last_Tx_Tick;
#else
static uint32_t App_Last_Rx_Tick;
#endif

#if APP_ROLE_TX_SENSOR
I2C_HandleTypeDef hi2c2;
static void STS30_Init(void);
static HAL_StatusTypeDef STS30_StartConversion(void);
static void SendTemperatureFrame(int16_t temp_x10);
#endif

static uint8_t TempFrameChecksum(uint8_t type, int16_t temp_x10);
#if APP_ROLE_TX_SENSOR
static void BuildTempFrame(uint8_t *buf, int16_t temp_x10);
#else
static uint8_t ParseTempFrame(const uint8_t *buf, uint8_t size, int16_t *temp_x10);
#endif
static void FormatTempString(uint8_t *buf, const char *prefix, int16_t temp_x10);

void BSP_Init()
{
    OLED_Init();
    BSP_RADIO_Init();
    LORA_Init();

#if APP_ROLE_TX_SENSOR
    STS30_Init();
    State = STATE_LORA_TX;
    App_Last_Tx_Tick = HAL_GetTick() - TEMP_SEND_PERIOD_MS;
#else
    State = STATE_LORA_RX;
    App_Last_Rx_Tick = HAL_GetTick();
#endif
    Old_State = STATE_MAIN;
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

#if APP_ROLE_TX_SENSOR
    if(App_Temp_Valid)
    {
        FormatTempString(oled_buf_Line1, "TX", App_Temp_X10);
    }
    else
    {
        sprintf((char *)oled_buf_Line1, "T:--.-C");
    }
    sprintf((char *)oled_buf_Line2, "Auto Send");
#else
    if(App_Temp_Valid && ((HAL_GetTick() - App_Last_Rx_Tick) <= TEMP_RX_TIMEOUT_MS))
    {
        FormatTempString(oled_buf_Line1, "RX", App_Temp_X10);
    }
    else
    {
        App_Temp_Valid = 0;
        sprintf((char *)oled_buf_Line1, "Waiting...");
    }
    sprintf((char *)oled_buf_Line2, "LoRa RX");
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

    if(KEY.KEY_down == ASW1)
    {
        if(State == STATE_MAIN) State = STATE_LORA_TX;
        else if(State == STATE_LORA_TX) State = STATE_LORA_RX;
        else if(State == STATE_LORA_RX) State = STATE_MAIN;
    }

#if APP_ROLE_TX_SENSOR
    if(KEY.KEY_down == ASW2)
    {
        int16_t temp_x10;

        if(STS30_ReadTempX10(&temp_x10) == HAL_OK)
        {
            App_Temp_X10 = temp_x10;
            App_Temp_Valid = 1;
            SendTemperatureFrame(temp_x10);
            App_Last_Tx_Tick = HAL_GetTick();
        }
        else
        {
            App_Temp_Valid = 0;
        }
    }
#endif
}

void LoRa_Proc(void)
{
#if APP_ROLE_TX_SENSOR
    uint32_t now = HAL_GetTick();
    int16_t temp_x10;

    if((now - App_Last_Tx_Tick) < TEMP_SEND_PERIOD_MS) return;
    App_Last_Tx_Tick = now;

    if(STS30_ReadTempX10(&temp_x10) == HAL_OK)
    {
        App_Temp_X10 = temp_x10;
        App_Temp_Valid = 1;
        SendTemperatureFrame(temp_x10);
    }
    else
    {
        App_Temp_Valid = 0;
    }
#else
    uint8_t lora_rx[8] = {0};
    uint8_t lora_size = 0;
    int16_t temp_x10;

    if(LORA_Rx(lora_rx, &lora_size) == 0) return;

    HAL_GPIO_WritePin(GPIOB, AL2_Pin, GPIO_PIN_RESET);
    if(ParseTempFrame(lora_rx, lora_size, &temp_x10))
    {
        App_Temp_X10 = temp_x10;
        App_Temp_Valid = 1;
        App_Last_Rx_Tick = HAL_GetTick();
        LoRa_RX_Buf[0] = lora_rx[0];
        LoRa_RX_Buf[1] = lora_rx[1];
        LoRa_RX_Buf[2] = lora_rx[2];
        LoRa_RX_Buf[3] = lora_rx[3];
        LoRa_RX_Buf[4] = lora_rx[4];
        LoRa_RX_Buf[5] = lora_rx[5];
    }
    HAL_GPIO_WritePin(GPIOB, AL2_Pin, GPIO_PIN_SET);
#endif
}

#if APP_ROLE_TX_SENSOR
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

HAL_StatusTypeDef STS30_ReadTempX10(int16_t *temp_x10)
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

static void SendTemperatureFrame(int16_t temp_x10)
{
    BuildTempFrame(LoRa_TX_Buf, temp_x10);
    HAL_GPIO_WritePin(GPIOB, AL1_Pin, GPIO_PIN_RESET);
    LORA_Tx(LoRa_TX_Buf, LORA_TEMP_FRAME_SIZE);
    HAL_GPIO_WritePin(GPIOB, AL1_Pin, GPIO_PIN_SET);
}
#endif

static uint8_t TempFrameChecksum(uint8_t type, int16_t temp_x10)
{
    uint8_t temp_hi = (uint8_t)(((uint16_t)temp_x10 >> 8) & 0xFFU);
    uint8_t temp_lo = (uint8_t)((uint16_t)temp_x10 & 0xFFU);

    return (uint8_t)(type ^ temp_hi ^ temp_lo);
}

#if APP_ROLE_TX_SENSOR
static void BuildTempFrame(uint8_t *buf, int16_t temp_x10)
{
    buf[0] = LORA_TEMP_FRAME_HEAD;
    buf[1] = LORA_TEMP_FRAME_TYPE;
    buf[2] = (uint8_t)(((uint16_t)temp_x10 >> 8) & 0xFFU);
    buf[3] = (uint8_t)((uint16_t)temp_x10 & 0xFFU);
    buf[4] = TempFrameChecksum(LORA_TEMP_FRAME_TYPE, temp_x10);
    buf[5] = LORA_TEMP_FRAME_TAIL;
}
#else

static uint8_t ParseTempFrame(const uint8_t *buf, uint8_t size, int16_t *temp_x10)
{
    int16_t value;

    if(buf == 0 || temp_x10 == 0) return 0;
    if(size != LORA_TEMP_FRAME_SIZE) return 0;
    if(buf[0] != LORA_TEMP_FRAME_HEAD || buf[5] != LORA_TEMP_FRAME_TAIL) return 0;
    if(buf[1] != LORA_TEMP_FRAME_TYPE) return 0;

    value = (int16_t)(((uint16_t)buf[2] << 8) | buf[3]);
    if(buf[4] != TempFrameChecksum(buf[1], value)) return 0;

    *temp_x10 = value;
    return 1;
}
#endif

static void FormatTempString(uint8_t *buf, const char *prefix, int16_t temp_x10)
{
    int16_t abs_temp = temp_x10;

    if(abs_temp < 0) abs_temp = (int16_t)-abs_temp;

    if(temp_x10 < 0)
    {
        sprintf((char *)buf, "%s T:-%d.%dC", prefix, abs_temp / 10, abs_temp % 10);
    }
    else
    {
        sprintf((char *)buf, "%s T:%d.%dC", prefix, abs_temp / 10, abs_temp % 10);
    }
}
