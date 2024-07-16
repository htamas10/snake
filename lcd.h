#pragma once
#ifndef _GLCD_H__
#define _GLCD_H__

void Init_LCD();
void LCD_Write(char cs, char d_i, char data);
void LCD_Clear();
void DataToPinMUX(char data);
void LCD_DrawRect(char x, char y, char data);
void LCD_Write_Char(char cPlace, char cX, char cY);
void LCD_WriteString(const char *string, char Y, char X);

#endif
