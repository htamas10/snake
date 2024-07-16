#include "stm32f4xx.h"
#include "stm32f4xx_hal.h"
#include "stm32f4xx_hal_cortex.h"
#include "main.h"
#include "lcd.h"
#include "fontdata.h"

//LCD inicializáció függvény
void Init_LCD() {
	HAL_GPIO_WritePin(LCD_RW_GPIO_Port, LCD_RW_Pin, RESET);			//R/W = 0, mivel írni akarunk
	HAL_GPIO_WritePin(LCD_EN_GPIO_Port, LCD_EN_Pin, SET);			//Enable = 1
	HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, RESET);		//RST = 0 (alacsony aktív)
	HAL_GPIO_WritePin(IC_EN_GPIO_Port, IC_EN_Pin, RESET);			//OE = 0
																	//Octal bufferek Output Enable jele (alacsony aktív)

	HAL_GPIO_WritePin(LCD_EN_GPIO_Port, LCD_EN_Pin, RESET);			//Enable = 0, ezt majd az irást megvalósító függvény kezeli
	HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, SET);			//RST = 1

	LCD_Write(3, 0, 0x3F);											//Bekapcsoljuk mindkét kijelzőrészt (cs1, cs2)
	LCD_Write(3, 0, 0xC0);											//Beállítjuk a starting line addresst 0-ra
	LCD_Clear();													//Cleareljük a kijelzőt
}

//Az adott 8 bites data változót fordítja le a pineknek megfelelően
void DataToPinMUX(char data) {
	char select = 0b00000001;										//maszkolást segítő változó

	HAL_GPIO_WritePin(DB0_GPIO_Port, DB0_Pin, (data & select));	    //LSB-vel kezdünk, ez lesz a DB0 pin
	select <<= 1;													//Shifteljük a maszkolóváltozót, ezzel haladunk az MSB felé
	HAL_GPIO_WritePin(DB1_GPIO_Port, DB1_Pin, (data & select));
	select <<= 1;
	HAL_GPIO_WritePin(DB2_GPIO_Port, DB2_Pin, (data & select));
	select <<= 1;
	HAL_GPIO_WritePin(DB3_GPIO_Port, DB3_Pin, (data & select));
	select <<= 1;
	HAL_GPIO_WritePin(DB4_GPIO_Port, DB4_Pin, (data & select));
	select <<= 1;
	HAL_GPIO_WritePin(DB5_GPIO_Port, DB5_Pin, (data & select));
	select <<= 1;
	HAL_GPIO_WritePin(DB6_GPIO_Port, DB6_Pin, (data & select));
	select <<= 1;
	HAL_GPIO_WritePin(DB7_GPIO_Port, DB7_Pin, (data & select));		//Végül az MSB-t (DB7) maszkoljuk
}

//LCD írást megvalósító függvény
void LCD_Write(char cs, char di, char data) {						//cs a chipselect jel (alacsony aktív)
	switch (cs) {													//cs = 1: cs1  /  cs = 2: cs2  /  cs = 3: cs1,cs2
	case 1:
		HAL_GPIO_WritePin(LCD_CS1_GPIO_Port, LCD_CS1_Pin, SET);
		break;
	case 2:
		HAL_GPIO_WritePin(LCD_CS2_GPIO_Port, LCD_CS2_Pin, SET);
		break;
	case 3:
		HAL_GPIO_WritePin(LCD_CS1_GPIO_Port, LCD_CS1_Pin, SET);
		HAL_GPIO_WritePin(LCD_CS2_GPIO_Port, LCD_CS2_Pin, SET);
	}

	switch (di) {													//di = 1 -> adatot küldünk
	case 0:															//di = 0 -> parancsot küldünk
		HAL_GPIO_WritePin(LCD_DI_GPIO_Port, LCD_DI_Pin, RESET);
		break;
	case 1:
		HAL_GPIO_WritePin(LCD_DI_GPIO_Port, LCD_DI_Pin, SET);
		break;
	}

	HAL_GPIO_WritePin(LCD_EN_GPIO_Port, LCD_EN_Pin, SET);			//Enable = 1

	DataToPinMUX(data);												//Konvertáljuk a datát a pinekre
	Sys_DelayUs(1);													//Várunk ~1 us-t, hogy az adatok megfelelően stabilak legyenek

	HAL_GPIO_WritePin(LCD_EN_GPIO_Port, LCD_EN_Pin, RESET);			//Enable lefutó élére veszi ki az LCD vezérlő az adatokat az input regiszterből
	Sys_DelayUs(1);													//Várunk ~1 us-t, mivel az Enable nagyjából ennyi ideig alacsonynak kell lennie

	HAL_GPIO_WritePin(LCD_CS1_GPIO_Port, LCD_CS1_Pin, RESET);			//Visszaállítjuk a chipselect jeleket
	HAL_GPIO_WritePin(LCD_CS2_GPIO_Port, LCD_CS2_Pin, RESET);
}

//LCD kijelzőjének törlése
void LCD_Clear() {
	char x, y;

	for (x = 0; x < 8; x++) {				//Először a 'page'-eken megyünk végig amiből 8 db van
		LCD_Write(3, 0, 0x40);				//Parancsot küldünk (NT7108 LCD kontroller adatlapjából kiderül, miért pont ezeket)
		LCD_Write(3, 0, (0xB8 | x));
		for (y = 0; y < 64; y++) {			//A 'page'-eken belül pedig végigmegyünk a 64-64 pixelen mindkét kijelzőn egyszerre
			LCD_Write(3, 1, 0x00);			//Mindenhova 0 kerül, azaz a pageken belüli egy oszlopban lévő 8 db pixel "kikapcsol"
		}
	}
}

//String kiírást megvalósítő függvény, mely a karakterkiíró függvényt használja
void LCD_WriteString(const char *string, char Y, char X) {
	char temp = 0;
	int i = 0;
	while (string[i] != '\0') {						//Végigmegyünk a stringen, amíg nem találkozunk a lezáró nullával
		temp = string[i];							//Egyenként írjuk ki a karaktereket
		LCD_Write_Char(temp - 32, X, Y + 6 * i);
		i++;
	}
}

//Karakter kiíró függvény az LCD-hez
void LCD_Write_Char(char cPlace, char cX, char cY) {
	char i = 0;
	char chip = 1;
	if (cY >= 64) {									//Ha cY >= 64, akkor már a második kijelzőn vagyunk (cs2), mivel összesen 128 pixelünk van (64 - 64)
		chip = 2;
		cY -= 64;
	}
	LCD_Write(chip, 0, (0x40 | cY));				//Elküldjük az íráshoz szükséges parancsokat
	LCD_Write(chip, 0, (0xB8 | cX));
	for (i = 0; i < 5; i++) {
		if (cY + i >= 64) {
			chip = 2;
			LCD_Write(chip, 0, (0x40 | (cY + i - 64)));
			LCD_Write(chip, 0, (0xB8 | cX));
		}
		LCD_Write(chip, 1, fontdata[cPlace * 5 + i]);
	}
}

//Egy 4x4-es négyzetet rajzolhatunk ki vele a 128x64 pixelből álló LCD-re (koordináták emiatt x: 0-15, y: 0-31 ig mehetnek)
void LCD_DrawRect(char x, char y, char data) {
	char cs = 1;

	if (y > 15) {							//y > 15, akkor a cs2 kell
		cs = 2;
		y -= 16;
	}

	LCD_Write(cs, 0, (0x40 | y * 4));
	LCD_Write(cs, 0, (0xB8 | x));

	for (int i = 0; i < 4; i++) {			//Először egy 'page'-en belüli oszlopot rajzolunk ki (itt a 'page' alatt nem az eredeti 8 db 'page'-et értjük,
		LCD_Write(cs, 1, data);				//hanem a 64 / 4 = 16 új lapot (egy eredeti lap igazából 2 darabnak számít, mivel egy page 8 pixel 'széles',
	}										//de nekünk 4 pixel széles a négyzetünk), majd ezt megismételjük 4-szer, hogy 4x4-es négyzetet kapjunk
											//Y address automatikusan egyel nő, minden oszlopba való rajzolás után, így azzal nem kell foglalkoznunk.
}

