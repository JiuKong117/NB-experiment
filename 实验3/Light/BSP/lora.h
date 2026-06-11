#ifndef __LORA_H__
#define __LORA_H__
#include "main.h"
#include "stdio.h"
#include "gpio.h"

void LORA_Init(void);
void LORA_Tx(uint8_t *ucBuf, uint8_t ucSize);
uint8_t LORA_Rx(uint8_t *ucBuf, uint8_t *ucSize);

typedef enum
{
    RADIO_SWITCH_RX     = 1,
    RADIO_SWITCH_TX     = 2,
} BSP_RADIO_Switch_TypeDef;

void BSP_RADIO_Init(void);
void BSP_RADIO_ConfigRFSwitch(BSP_RADIO_Switch_TypeDef Config);



#endif /* __LORA_H__ */
