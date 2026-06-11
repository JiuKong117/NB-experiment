/*************************************************
Copyright (c)Guoxinchangtian Technologies Co., Ltd. 2008-2019. All rights reserved.
File name: oled.h
Author:      Version:   Date:2019.10.10
Description: OLED显示屏的读写操作的头文件，罗列了对OLED屏读写操作函数。
Others:
History:

1. Date:
Author:
Modification:
*************************************************/

#ifndef __OLED_H
#define __OLED_H

#include "main.h"

#define Max_Column	128
#define Max_Row		32


typedef enum
{
    TYPE_COMMAND = 0,
    TYPE_DATA = 0x40
} OLED_TYPE;

/**
  * @brief OLED屏幕清屏。
  * @param None
  * @retval None
  */
void OLED_Clear(void);

/**
  * @brief OLED屏幕初始化。
  * @param None
  * @retval None
  */
void OLED_Init(void);

/**
  * @brief OLED屏幕显示一个字符。
  * @param x 横向选择位置；
					 y 纵向选择位置；
					 chr 需要显示的字符；
					 size 需要显示的字符大小；

  * @retval None
  */
void OLED_ShowChar(uint8_t x, uint8_t y, uint8_t chr, uint8_t size);

/**
  * @brief OLED屏幕显示一个字符串。
  * @param x 横向选择位置；
					 y 纵向选择位置；
					 chr 需要显示的字符串；
					 size 需要显示的字符大小；

  * @retval None
  */
void OLED_ShowString(uint8_t x, uint8_t y, uint8_t *chr, uint8_t size);


#endif
