#include "task.h"

#define LORA_LIGHT_FRAME_HEAD 0xAAU
#define LORA_LIGHT_FRAME_TAIL 0x55U
#define LORA_LIGHT_FRAME_TYPE 0x02U
#define LORA_LIGHT_FRAME_SIZE 6U
#define LIGHT_ADC_GPIO_PORT GPIOB
#define LIGHT_ADC_GPIO_PIN GPIO_PIN_3
#define LIGHT_ADC_CHANNEL ADC_CHANNEL_2

Interface State = STATE_MAIN;
Interface Old_State = STATE_MAIN;
Task_Time_Adj Task_Time;
KEY_State KEY;

#if APP_ROLE_TX_SENSOR
uint8_t LoRa_TX_Buf[LORA_LIGHT_FRAME_SIZE];
#else
uint8_t LoRa_RX_Buf[LORA_LIGHT_FRAME_SIZE];
#endif
uint16_t Time_Num;
uint8_t Key_Flag;

static uint16_t App_Light_Raw;
static uint8_t App_Light_Valid;
#if APP_ROLE_TX_SENSOR
static uint32_t App_Last_Tx_Tick;
#else
static uint32_t App_Last_Rx_Tick;
#endif

#if APP_ROLE_TX_SENSOR
static void LightSensor_Init(void);
static void SendLightFrame(uint16_t light_raw);
#endif

static uint8_t LightFrameChecksum(uint8_t type, uint16_t light_raw);
#if APP_ROLE_TX_SENSOR
static void BuildLightFrame(uint8_t *buf, uint16_t light_raw);
#else
static uint8_t ParseLightFrame(const uint8_t *buf, uint8_t size, uint16_t *light_raw);
#endif
static void FormatLightString(uint8_t *buf, const char *prefix, uint16_t light_raw);

void BSP_Init()
{
    OLED_Init();
    BSP_RADIO_Init();
    LORA_Init();

#if APP_ROLE_TX_SENSOR
    LightSensor_Init();
    State = STATE_LORA_TX;
    App_Last_Tx_Tick = HAL_GetTick() - LIGHT_SEND_PERIOD_MS;
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
    if(App_Light_Valid)
    {
        FormatLightString(oled_buf_Line1, "TX", App_Light_Raw);
    }
    else
    {
        sprintf((char *)oled_buf_Line1, "L:----");
    }
    sprintf((char *)oled_buf_Line2, "Auto Send");
#else
    if(App_Light_Valid && ((HAL_GetTick() - App_Last_Rx_Tick) <= LIGHT_RX_TIMEOUT_MS))
    {
        FormatLightString(oled_buf_Line1, "RX", App_Light_Raw);
    }
    else
    {
        App_Light_Valid = 0;
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
        uint16_t light_raw;

        if(LightSensor_ReadRaw(&light_raw) == HAL_OK)
        {
            App_Light_Raw = light_raw;
            App_Light_Valid = 1;
            SendLightFrame(light_raw);
            App_Last_Tx_Tick = HAL_GetTick();
        }
        else
        {
            App_Light_Valid = 0;
        }
    }
#endif
}

void LoRa_Proc(void)
{
#if APP_ROLE_TX_SENSOR
    uint32_t now = HAL_GetTick();
    uint16_t light_raw;

    if((now - App_Last_Tx_Tick) < LIGHT_SEND_PERIOD_MS) return;
    App_Last_Tx_Tick = now;

    if(LightSensor_ReadRaw(&light_raw) == HAL_OK)
    {
        App_Light_Raw = light_raw;
        App_Light_Valid = 1;
        SendLightFrame(light_raw);
    }
    else
    {
        App_Light_Valid = 0;
    }
#else
    uint8_t lora_rx[8] = {0};
    uint8_t lora_size = 0;
    uint16_t light_raw;

    if(LORA_Rx(lora_rx, &lora_size) == 0) return;

    HAL_GPIO_WritePin(GPIOB, AL2_Pin, GPIO_PIN_RESET);
    if(ParseLightFrame(lora_rx, lora_size, &light_raw))
    {
        App_Light_Raw = light_raw;
        App_Light_Valid = 1;
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
static void LightSensor_Init(void)
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};

    __HAL_RCC_GPIOB_CLK_ENABLE();

    GPIO_InitStruct.Pin = LIGHT_ADC_GPIO_PIN;
    GPIO_InitStruct.Mode = GPIO_MODE_ANALOG;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    HAL_GPIO_Init(LIGHT_ADC_GPIO_PORT, &GPIO_InitStruct);
}

HAL_StatusTypeDef LightSensor_ReadRaw(uint16_t *light_raw)
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

static void SendLightFrame(uint16_t light_raw)
{
    BuildLightFrame(LoRa_TX_Buf, light_raw);
    HAL_GPIO_WritePin(GPIOB, AL1_Pin, GPIO_PIN_RESET);
    LORA_Tx(LoRa_TX_Buf, LORA_LIGHT_FRAME_SIZE);
    HAL_GPIO_WritePin(GPIOB, AL1_Pin, GPIO_PIN_SET);
}
#endif

static uint8_t LightFrameChecksum(uint8_t type, uint16_t light_raw)
{
    uint8_t light_hi = (uint8_t)((light_raw >> 8) & 0xFFU);
    uint8_t light_lo = (uint8_t)(light_raw & 0xFFU);

    return (uint8_t)(type ^ light_hi ^ light_lo);
}

#if APP_ROLE_TX_SENSOR
static void BuildLightFrame(uint8_t *buf, uint16_t light_raw)
{
    buf[0] = LORA_LIGHT_FRAME_HEAD;
    buf[1] = LORA_LIGHT_FRAME_TYPE;
    buf[2] = (uint8_t)((light_raw >> 8) & 0xFFU);
    buf[3] = (uint8_t)(light_raw & 0xFFU);
    buf[4] = LightFrameChecksum(LORA_LIGHT_FRAME_TYPE, light_raw);
    buf[5] = LORA_LIGHT_FRAME_TAIL;
}
#else

static uint8_t ParseLightFrame(const uint8_t *buf, uint8_t size, uint16_t *light_raw)
{
    uint16_t value;

    if(buf == 0 || light_raw == 0) return 0;
    if(size != LORA_LIGHT_FRAME_SIZE) return 0;
    if(buf[0] != LORA_LIGHT_FRAME_HEAD || buf[5] != LORA_LIGHT_FRAME_TAIL) return 0;
    if(buf[1] != LORA_LIGHT_FRAME_TYPE) return 0;

    value = (uint16_t)(((uint16_t)buf[2] << 8) | buf[3]);
    if(buf[4] != LightFrameChecksum(buf[1], value)) return 0;

    *light_raw = value;
    return 1;
}
#endif

static void FormatLightString(uint8_t *buf, const char *prefix, uint16_t light_raw)
{
    sprintf((char *)buf, "%s L:%4u", prefix, (unsigned int)light_raw);
}
