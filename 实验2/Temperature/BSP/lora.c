#include "lora.h"

extern SUBGHZ_HandleTypeDef hsubghz;

/**
 * @brief Set the radio in LoRa?, FSK or Long Range FHSS mode.
 *
 * @param ucPT 0-(G)FSK packet type
 *             1-LoRa mode
 *             3-Long Range FHSS
 * @return None.
 */
void LORA_SetPacketType(uint8_t ucPT)
{
    uint8_t data = ucPT;
    HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_PACKETTYPE, &data, 1);
}

/**
 * @brief Set  LoRa? packets parameters
 *
 * @param usPL ,number of symbols sent as preamble.
 * @param ucHT ,HeaderType, 0-1
 * @param ucDL ,Size of the payload (in bytes) to transmit or maximum size of the
                payload that the receiver can accept
 * @param ucCM ,CRC on or off.
 * @param ucIQ ,InvertIQ or Not.
 */
void LORA_SetPacketParams(uint16_t usPL, uint8_t ucHT, uint8_t ucDL, uint8_t ucCM, uint8_t ucIQ)
{
    uint8_t data[6];
    data[0] = usPL >> 8;
    data[1] = usPL;
    data[2] = ucHT;
    data[3] = ucDL;
    data[4] = ucCM;
    data[5] = ucIQ;
    HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_PACKETPARAMS, data, 6);
}

/**
 * @brief Set LoRa? Modulation Parameters
 *
 * @param ucSF ,5-6-7-8-9-10-11-12, SF5-SF12
 * @param ucBW ,0-8-1-9-2-10-3-4-5-6, LORA_BW
 * @param ucCR ,1-2-3-4, LORA_CR_4_5 - LORA_CR_4_8
 * @param ucLO ,0-1, LowDataRateOptimize
 * @return None.
 */
void LORA_SetModulationParams(unsigned char ucSF, unsigned char ucBW, unsigned char ucCR, unsigned char ucLO)
{
    uint8_t data[4];

    data[0] = ucSF;
    data[1] = ucBW;
    data[2] = ucCR;
    data[3] = ucLO;
    HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_MODULATIONPARAMS, data, 4);
}

/**
 * @brief Set Lora frequency band.
 *
 * @param frequency , frequency in MHz.
 * @return None.
 */
void LORA_SetRfFrequency(uint32_t MHz )
{
    uint8_t data[4];
    uint32_t chan  = (MHz << 20);

    data[0] = ( uint8_t )( ( chan >> 24 ) & 0xFF );
    data[1] = ( uint8_t )( ( chan >> 16 ) & 0xFF );
    data[2] = ( uint8_t )( ( chan >> 8 ) & 0xFF );
    data[3] = ( uint8_t )( chan & 0xFF );
    HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_RFFREQUENCY, data, 4 );
}


/**
 * @brief  Set the device to SLEEP mode.
 *
 * @param mode ,0 cold start.
 *              4 warm start.
 * @return None.
 */
void LORA_SetSleep(uint8_t mode)
{
    uint8_t data = mode;
    HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_SLEEP, &data, 1);
}

/**
 * @brief  Set the device to STANDBY mode.
 *
 * @param mode ,0 RC standby mode.
 *              1 XTAL standby mode.
 * @return None.
 */
void LORA_SetStandby(uint8_t mode)
{
    uint8_t data = mode;
    HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_STANDBY, &data, 1);
}

/**
 * @brief Set the device to TX or RX mode.
 *
 * @param mode ,0 Rx mode.
 *              1 Tx mode.
 * @return None.
 */
void LORA_SetTxRx(uint8_t mode)
{
    uint8_t data[3] = {0xFF, 0xFF, 0xFF};

    if(mode == 0)
    {
        //Rx Continuous mode
        BSP_RADIO_ConfigRFSwitch(RADIO_SWITCH_RX);
        HAL_Delay(1);
        HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_RX, data, 3);
    }
    else
    {
        data[0] = data[1] = data[2] = 0;
        BSP_RADIO_ConfigRFSwitch(RADIO_SWITCH_TX);
        HAL_Delay(1);
        HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_TX, data, 3);
    }
}

/**
 * @brief Set the Power Amplifier configuration.
 *
 * @param duty ,controls the duty cycle < 0x04.
 * @param hp_max ,selects the size of the PA. < 0x07
 * @param device ,0x00.
 * @param lut ,0x01.
 */
void LORA_SetPA(uint8_t duty, uint8_t hp_max, uint8_t device, uint8_t lut)
{
    uint8_t data[4];

    data[0] = duty;
    data[1] = hp_max;
    data[2] = device;
    data[3] = lut;

    HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_PACONFIG, data, 4);
}

/**
 * @brief Get the device status.
 *
 * @param None.
 * @return uint8_t status, device status.
 */
uint8_t LORA_GetStatus(void)
{
    uint8_t status;
    HAL_SUBGHZ_ExecGetCmd(&hsubghz, RADIO_GET_STATUS, &status, 1);
    return status;
}

/**
 * @brief Sets the TX parameter power and the TX ramping time.
 *
 * @param power ,0xF7 ~ 0x16
 * @param ramptime ,SET_RAMP_10U - SET_RAMP_3400U
 */
void LORA_SetTxPara(uint8_t power, uint8_t ramptime)
{
    uint8_t data[2];
    data[0] = power;
    data[1] = ramptime;
    HAL_SUBGHZ_ExecSetCmd(&hsubghz, RADIO_SET_TXPARAMS, data, 2);

}

/**
 * @brief LoRa Initialization function.
 *        Default setting:
 *          - LoRa mode.
 *          - Frequency 476MHz.
 * @note   This function must be called before using other functions.
 * @param None.
 * @return None.
 */
void LORA_Init(void)
{
    LORA_SetSleep(0);
    HAL_Delay(5);
    LORA_SetStandby(0);
    HAL_Delay(5);
    LORA_SetStandby(1);
    HAL_Delay(5);

    LORA_SetPacketType(1);
    LORA_SetRfFrequency(433);

    LORA_SetPA(1, 0, 1, 1);	//14DB
    LORA_SetTxPara(13, 0x4);


    LORA_SetModulationParams(7, 4, 1, 0);
    LORA_SetPacketParams(8, 0, 8, 1, 0);
    LORA_SetTxRx(0);
}

/**
 * @brief LoRa Send
 *
 * @param ucBuf ,A pointer to the data buffer.
 * @param ucSize ,send data length.
 */
void LORA_Tx(uint8_t *ucBuf, uint8_t ucSize)
{
    uint16_t i = 0;
    uint8_t status = 0;
	
	LORA_SetPacketParams(8, 0, ucSize, 1,0);        
    HAL_SUBGHZ_WriteBuffer(&hsubghz, 0, ucBuf, ucSize);
    LORA_SetTxRx(1);

    do{
        status = LORA_GetStatus();
                --i;
    }while((status != 0xac) && (i != 0));

    LORA_SetTxRx(0);
}

/**
 * @brief LoRa Receive
 *
 * @param ucBuff ,A pointer to the data buffer.
 * @param ucSize ,Receive data length.
 * @return 1, received data.
 *         0, no data.
 */
uint8_t LORA_Rx(uint8_t *ucBuff, uint8_t *ucSize)
{
    uint8_t status[2];

    status[0] = LORA_GetStatus();
    if (status[0] == 0xd4)
    {
        HAL_SUBGHZ_ExecGetCmd(&hsubghz, RADIO_GET_RXBUFFERSTATUS, status, 2);
        HAL_SUBGHZ_ReadBuffer(&hsubghz, status[1], ucBuff, status[0]);
        *ucSize = status[0];

        LORA_SetTxRx(0);
        return 1;
    }
    else
    {
        return 0;
    }
}






void BSP_RADIO_Init(void)                                               //��Ҫ�Լ���д�������ĺ���
{
    GPIO_InitTypeDef GPIO_InitStruct = {0};                             //PC13��ʼ��Ϊ�������

    __HAL_RCC_GPIOC_CLK_ENABLE();

    GPIO_InitStruct.Pin = GPIO_PIN_13;
    GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
    GPIO_InitStruct.Pull = GPIO_NOPULL;
    GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
    HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

    HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
}



void BSP_RADIO_ConfigRFSwitch(BSP_RADIO_Switch_TypeDef Config)          //PC13�л���ͬ��ƽ
{
    {
        switch (Config)
        {
        case RADIO_SWITCH_RX:
        {
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_RESET);
            break;
        }
        case RADIO_SWITCH_TX:
        {
            HAL_GPIO_WritePin(GPIOC, GPIO_PIN_13, GPIO_PIN_SET);
            break;
        }
        default:
            break;
        }
    }
}


