#ifndef __TASK_H
#define __TASK_H

#include "main.h"
#include "adc.h"
#include "i2c.h"
#include "oled.h"
#include "stdio.h"
#include "lora.h"

#define OLED_TIME 200
#define KEY_TIME 10
#define SENSOR_SEND_PERIOD_MS 1000U
#define SENSOR_RX_TIMEOUT_MS 5000U

#define APP_NODE_TEMP 1U
#define APP_NODE_LIGHT 2U

#ifndef APP_NODE_ROLE
#define APP_NODE_ROLE APP_NODE_TEMP
#endif

#define ASW_None 0x00
#define ASW1 0x01
#define ASW2 0x02

typedef enum
{
    STATE_MAIN = 0,
    STATE_TEMP_NODE,
    STATE_LIGHT_NODE
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

#endif
