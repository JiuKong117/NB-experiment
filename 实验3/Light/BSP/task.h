#ifndef __TASK_H
#define __TASK_H

#include "main.h"
#include "adc.h"
#include "oled.h"
#include "stdio.h"
#include "lora.h"

#define OLED_TIME 200
#define KEY_TIME 10
#define LIGHT_SEND_PERIOD_MS 1000U
#define LIGHT_RX_TIMEOUT_MS 5000U

#ifndef APP_ROLE_TX_SENSOR
#define APP_ROLE_TX_SENSOR 0
#endif

#define ASW_None 0x00
#define ASW1 0x01
#define ASW2 0x02

typedef enum
{
    STATE_MAIN = 0,
    STATE_LORA_TX,
    STATE_LORA_RX
} Interface;

extern Interface State;

typedef struct
{
    uint8_t Key_Time;
    uint8_t Oled_Time;
} Task_Time_Adj;

extern Task_Time_Adj Task_Time;

typedef struct
{
    uint8_t KEY_down;
    uint8_t KEY_up;
    uint8_t KEY_old;
} KEY_State;

extern uint16_t Time_Num;

void BSP_Init(void);
void OLED_Proc(void);
void KEY_Proc(void);
void LoRa_Proc(void);

#if APP_ROLE_TX_SENSOR
HAL_StatusTypeDef LightSensor_ReadRaw(uint16_t *light_raw);
#endif

#endif
