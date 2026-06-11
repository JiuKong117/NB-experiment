/*************************************************
Copyright (c)Guoxinchangtian Technologies Co., Ltd. 2008-2019. All rights reserved.
File name: oled.c
Author:      Version:   Date:2019.10.10
Description: 实现对LoRa模块的OLED显示屏的读写操作。
Others:
History:

1. Date:
Author:
Modification:
*************************************************/
#include "i2c.h"
#include "oled.h"
#include "font.h"
/* 设置位置（参考表3.6） */
void OLED_SetPos(uint8_t x, uint8_t y)
{
    OLED_Write(TYPE_COMMAND, x & 0x0f);
    OLED_Write(TYPE_COMMAND, 0x10 + ((x & 0xf0) >> 4));
    OLED_Write(TYPE_COMMAND, 0xB0 + y);
}
/* 清除屏幕 */
void OLED_Clear(void)
{
    uint8_t i, j;

    for(i = 0; i < 4; i++)
    {
        OLED_SetPos(0, i);
        for(j = 0; j < 128; j++)
            OLED_Write(TYPE_DATA, 0);
    }
}
/**
  * @brief OLED屏幕初始化。
  * @param None
  * @retval None
  */
void OLED_Init(void)
{
	HAL_Delay(100);
    
    OLED_Write(TYPE_COMMAND, 0xAE);	
    OLED_Write(TYPE_COMMAND, 0xD5);	
    OLED_Write(TYPE_COMMAND, 0x80);	
    OLED_Write(TYPE_COMMAND, 0xA8);	
    OLED_Write(TYPE_COMMAND, 0x1F);	
    OLED_Write(TYPE_COMMAND, 0xD3);	
    OLED_Write(TYPE_COMMAND, 0x00);	
    OLED_Write(TYPE_COMMAND, 0x40);
    OLED_Write(TYPE_COMMAND, 0x8D);
    OLED_Write(TYPE_COMMAND, 0x14);
    OLED_Write(TYPE_COMMAND, 0xA1);
    OLED_Write(TYPE_COMMAND, 0xC8);
    OLED_Write(TYPE_COMMAND, 0xDA);
    OLED_Write(TYPE_COMMAND, 0x00);
    OLED_Write(TYPE_COMMAND, 0x81);
    OLED_Write(TYPE_COMMAND, 0x8F);
    OLED_Write(TYPE_COMMAND, 0xD9);
    OLED_Write(TYPE_COMMAND, 0x1F);
    OLED_Write(TYPE_COMMAND, 0xDB);
    OLED_Write(TYPE_COMMAND, 0x40);
    OLED_Write(TYPE_COMMAND, 0xA4);
    
    OLED_Clear();										
    OLED_Write(TYPE_COMMAND, 0xAF);	
	HAL_Delay(100);
}

/**
  * @brief OLED屏幕显示一个字符。
  * @param x 横向选择位置；
					 y 纵向选择位置；
					 chr 需要显示的字符；
					 size 需要显示的字符大小；
					 @arg 16,选择8*16点阵大小字符。其他均选择6*8点阵字符
  * @retval None
  */
void OLED_ShowChar(uint8_t x, uint8_t y, uint8_t chr, uint8_t size)
{
    uint8_t  c, i;

    c = chr - ' ';
    if(x > Max_Column - 1)
    {
        x = 0;
        y = y + 2;
    }
    if(size == 16)
    {
        OLED_SetPos(x, y);
        for(i = 0; i < 8; i++)
            OLED_Write(TYPE_DATA, g_F8X16[c * 16 + i]);
        OLED_SetPos(x, y + 1);
        for(i = 0; i < 8; i++)
            OLED_Write(TYPE_DATA, g_F8X16[c * 16 + i + 8]);
    }
    else
    {
        OLED_SetPos(x, y);
        for(i = 0; i < 6; i++)
        {
            OLED_Write(TYPE_DATA, g_F6x8[c][i]);
        }
    }
}

/**
  * @brief OLED屏幕显示一个字符串。
  * @param x 横向选择位置；
					 y 纵向选择位置；
					 chr 需要显示的字符串；
					 size 需要显示的字符大小；

  * @retval None
  */
void OLED_ShowString(uint8_t x, uint8_t y, uint8_t *chr, uint8_t size)
{
    uint8_t j = 0;

    while(chr[j] != '\0')
    {
        OLED_ShowChar(x, y, chr[j], size);
        x += 8;
        if(x > 120)
        {
            x = 0;
            y += 2;
        }
        j++;
    }
}

