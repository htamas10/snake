#include "main.h"
#include "lcd.h"
#include "game.h"
#include "stdbool.h"
#include "stdlib.h"
#include "stdio.h"
#include "uart.h"

extern bool timerflag;
extern bool start;
extern UART_HandleTypeDef huart2;

char gameboard[16][32] = { 0 }; //A játék pályája: 16x32-es mivel 4x4 nagyságú négyzetekből áll, ha valahol 1-est tartalmaz akkor ott van négyzet, egyébként nincs
char pagedata[8][32] = { 0 };//Az eredeti 8 db 'page' adatmezőit tartalmazó 2D tömb

uint8_t score;						//A játékos pontjai (1 alma után +1 pont)
bool lcdcleared = false;
bool gameover = false;
bool win = false;

Menu menu;
Snake snake;
Point apple;
Point head;
Menu previousMenu = START;		//A menünek két 'jelenleg' kiválasztott állapota van (ezt kurzor jelzi és a Menu enum változó tárolja)

void Init_Game() {
	Init_Snake();
	Spawn_Apple();
	score = 0;
	gameover = false;
	menu = START;
}

//Kígyó felparaméterezése
void Init_Snake() {
	snake.length = 3;							//Kígyó kezdőértékei
	snake.speed = 400;//Abszolútértékben kisebb szám a valóságban nagyobb sebességet jelent
	snake.direction = LEFT;

	for (int i = 0; i < MAX_LENGTH; i++) {//A Snake struktúra egyik tagváltozója egy (x,y) pontokat tartalmazó tömb
		snake.positions[i].x = '?';	//Feltöltjük a pozíciókat tartalmazó tömböt 'invalid' értékekkel
		snake.positions[i].y = '?';
	}

	snake.positions[0].x = 8;//A kígyó 3 hosszú, így 3 db (x,y) pontot hozunk létre 'valid' adatokkal
	snake.positions[0].y = 17;

	snake.positions[1].x = 8;
	snake.positions[1].y = 16;

	snake.positions[2].x = 8;
	snake.positions[2].y = 15;
}

//Nullázza a gameboard 2D-s tömb elemeit
void Clear_Gameboard() {
	for (int i = 0; i < 16; i++) {
		for (int j = 0; j < 32; j++) {
			gameboard[i][j] = 0;
		}
	}
}

//A kijelző frissítését megvalósító, valamint az LCD-re való kiírás logikáját megvalósító függvény
void Update_Screen() {
	Clear_Gameboard();							//Először töröjük a játékpályát

	for (int i = 0; i < MAX_LENGTH; i++) {//Ha valahol a kígyó pozícióját tartalmazó tömbben 'invalid' érték van, akkor ott
		if (snake.positions[i].x == '?' && snake.positions[i].y == '?') {//0 lesz a gameboard[][] értéke, egyébként pedig 1
			gameboard[snake.positions[i].x][snake.positions[i].y] = 0;
		} else {
			gameboard[snake.positions[i].x][snake.positions[i].y] = 1;
		}
	}

	head.x = snake.positions[snake.length - 1].x;
	head.y = snake.positions[snake.length - 1].y;
	gameboard[head.x][head.y] = 3;//A fej és az alma pozíciója különleges, így a fejet egy 3-as, az almát pedig 2-es reprezentálja
	gameboard[apple.x][apple.y] = 2;//a játékpálya tömbjében (így megkülönböztetheőek a kígyó testétől, amiket 1-es szimbolizál)

	Create_Pagemask();				//Elkészítjük a saját 'page'-eink adatait

	for (int i = 0; i < 8; i++) {//Végül négyzetet rajzolunk oda, ahol a gameboard[x][y] = 1/2/3-at tartalmaz
		for (int j = 0; j < 32; j++) {
			LCD_DrawRect(i, j, pagedata[i][j]);
		}
	}
}

//A gyártói 8 db 'page'-ből mi látszólag 16-ot használunk, így 1 db 'page' igazából 2-nek számít a mi szempontunkól
void Create_Pagemask() {
	int k = 0;

	for (int i = 0; i < 16; i += 2) {//A működés lényege, hogy ha 1 gyártói 'page'-en belül mi csak 1-et használunk akkor nem 8 pixelt
		for (int j = 0; j < 32; j++) {//kell megjelenítenünk hanem vagy a felső 4-et, vagy csak az alsó 4-et (vagy egyet se)
			if ((gameboard[i][j] == 1 || gameboard[i][j] == 2//Ezt úgy ellenőrizzük, hogy amennyiben a gameboard[i][j] egy adott j oszlopában állva azt tapasztaljuk,
			|| gameboard[i][j] == 3) && gameboard[i + 1][j] == 0)//hogy a jelenlegi i sorban van elem, az alattunk lévő i + 1 sorban szintén van elem akkor nekünk mind a
				pagedata[k][j] = 0x0f;//4 + 4 pixel kell a gyártói 'page'-en belül, tehát 0xFF-et írunk oda stb...

			else if (gameboard[i][j] == 0
					&& (gameboard[i + 1][j] == 1 || gameboard[i + 1][j] == 2//A logika hasonló a továbbiakban is, ezektől függ, hogy egy gyártói 'page'-re 0xFF,0x0F,0xF0 kerül
					|| gameboard[i + 1][j] == 3))
				pagedata[k][j] = 0xf0;

			else if ((gameboard[i][j] == 1 || gameboard[i][j] == 2
					|| gameboard[i][j] == 3)
					&& (gameboard[i + 1][j] == 1 || gameboard[i + 1][j] == 2
							|| gameboard[i + 1][j] == 3))
				pagedata[k][j] = 0xff;

			else
				pagedata[k][j] = 0;
		}
		k++;
	}
}

//A kígyó mozgatását megvalósítő függvény
void Move_Snake(int deltaX, int deltaY) {
	for (int i = 0; i < snake.length - 1; i++) {//Végigiterálunk a kígyó pozícióit tartalmazó tömbbön, a faroktól haladva a fejig (a snake.length - 1 elem)
		snake.positions[i].x = snake.positions[i + 1].x; //Egy adott i. elem az előtte lévő elem pozícióját veszi fel, így olyan mintha a kígyónk mozogna a pályán
		snake.positions[i].y = snake.positions[i + 1].y;
	}

	snake.positions[snake.length - 1].x += deltaX;//A fej pozícióját pedig a deltaX, illetve deltaY változóban tárolt érték szerint kell állítanunk
	snake.positions[snake.length - 1].y += deltaY;

	head.x = snake.positions[snake.length - 1].x;//Elmentjük a fej pozícióját, hogy átláthatóbban tudjuk használni
	head.y = snake.positions[snake.length - 1].y;
	bool collision = gameboard[head.x][head.y] == 1;//Vizsgáljuk, hogy a fej (3-as a gameboard[][]-ban) nem ütközött-e testrésszel (amiket pedig 1-es szimbolizál)
													//Ha ütköztünk, akkor a 'collision' változó 'true' lesz
	if ((head.x == 255 || head.x == 16 || head.y == 255 || head.y == 32) //Ha testnek ütköztünk a fejjel, vagy pedig a pálya széleit elértük akkor véget ér a játék
	|| collision) {
		gameover = true;
	}

}

//Vizsgáljuk, hogy ettünk-e almát
void Is_Apple_Eaten() {
	if (snake.positions[snake.length - 1].x == apple.x//Amennyiben a fej pozíciója épp egy alma pozíciójára esik, akkor ettünk almát
	&& snake.positions[snake.length - 1].y == apple.y) {

		Increase_Snake();			//Ha ettünk akkor növeljük a kígyót...
		Spawn_Apple();				//Létrehozunk egy új almát...

		score++;					//Valamint a pontszámot is növeljük...
		HAL_UART_Transmit(&huart2, &score, sizeof(char), 10);

		if (score % 10 == 0 && score <= 100) {//Minden 10. pontgyűjtésnél növelünk a kígyó sebességén, hogy nehezítsük a játékot
			snake.speed -= 15;//Ha 10x növekedett már a sebesség akkor nem nehezítünk tovább

			HAL_GPIO_WritePin(USER_LED_GPIO_Port, USER_LED_Pin, SET);//Dupla villogással jelezzük a nehézség növelésének pillanatát
			Sys_DelayMs(25);
			HAL_GPIO_WritePin(USER_LED_GPIO_Port, USER_LED_Pin, RESET);
			Sys_DelayMs(25);
			HAL_GPIO_WritePin(USER_LED_GPIO_Port, USER_LED_Pin, SET);
			Sys_DelayMs(25);
			HAL_GPIO_WritePin(USER_LED_GPIO_Port, USER_LED_Pin, RESET);
		}

		else {
			HAL_GPIO_WritePin(USER_LED_GPIO_Port, USER_LED_Pin, SET);//Szimpa villanással jelezzük, hogy felszedtük az almát
			Sys_DelayMs(50);
			HAL_GPIO_WritePin(USER_LED_GPIO_Port, USER_LED_Pin, RESET);
		}
	}

}

//Egy egyszerű függvény, ami a Move_Snake()-et hívja
void Check_Direction() {
	switch (snake.direction) {//Ez a függvény csak az timerflag által biztosított ütemekben fut le
	case LEFT://A snake.direction változót az external IT kezelő függvény állíthatja, vagy pedig az UART-on kapott adat
		Move_Snake(0, -1);//Első paraméter a deltaX, második a deltaY (a kijelző origója a bal felső rész)
		break;

	case UP:
		Move_Snake(-1, 0);
		break;

	case RIGHT:
		Move_Snake(0, 1);
		break;

	case DOWN:
		Move_Snake(1, 0);
		break;
	}
}

//Ha ettünk almát, vagy kezdődött a játék akkor létrehozunk egy új almát
void Spawn_Apple() {
	int x, y;
	do {
		srand(HAL_GetTick());			//A seed-et a HAL_GetTick() fv. adja meg
		x = rand() % (16 - 2) + 1;//Csak ott lehet alma, ahol a játékpálya 0-t tartalmaz (nincs ott semmi)
		y = rand() % (32 - 2) + 1;
	} while (gameboard[x][y] != 0);
	apple.x = x;
	apple.y = y;
}

//Ha almát ettünk akkor növeljük a kígyó méretét
void Increase_Snake() {
	for (int i = snake.length - 1; i > 0; i--) {//Most a fejtől haladva a farokig iterálunk végig
		snake.positions[i].x = snake.positions[i - 1].x;//Minden i. pozíció koordinátája mostantól az előtte lévő (i - 1) pont koordinátáját veszi fel
		snake.positions[i].y = snake.positions[i - 1].y;//Ezzel végsősoron növeljük a kígyó méretét, mivel az egész kígyót 1-gyel eltoltuk a tömbben felfele
	}

	snake.positions[snake.length].x = apple.x;//A fej pedig mostantól a snake.length koordinátájú pont lesz, ami épp az alma koordinátája
	snake.positions[snake.length].y = apple.y;
	snake.length++;									//Növeljük a kígyó méretét

	if (snake.length == MAX_LENGTH) {//Ha elértük a kígyó max méretet (128x64 = 512) akkor megnyertük a játékot
		gameover = true;
		win = true;
	}

}

//Ha vesztettünk vagy nyertünk akkor ez a függvény hívódik meg
void Game_Over() {
	if (!lcdcleared) {			//Törlünk egyszer a kijelzőn
		LCD_Clear();
		lcdcleared = true;
	}

	if (win) {													//Ha nyertünk...
		while (HAL_GPIO_ReadPin(BTN_RIGHT_GPIO_Port, BTN_RIGHT_Pin)) {
			LCD_WriteString("YOU WON!!!", 35, 3);
			LCD_WriteString("CONGRATULATIONS!", 18, 5);
			HAL_GPIO_WritePin(USER_LED_GPIO_Port, USER_LED_Pin, SET); //Gyors villogtatással jelezzük a játék végét
			Sys_DelayMs(100);
			HAL_GPIO_WritePin(USER_LED_GPIO_Port, USER_LED_Pin, RESET);
			Sys_DelayMs(25);
		}
	}

	else {													//Ha vesztettünk...
		LCD_WriteString("GAME OVER!", 34, 2);
		LCD_WriteString("Your score: ", 32, 3);

		int length = snprintf(NULL, 0, "%d", score);//Stringgé konvertáljuk a pontszámot, majd kijelezzük
		char str[10];
		snprintf(str, length + 1, "%d", score);
		LCD_WriteString(str, 63, 5);

		HAL_GPIO_TogglePin(USER_LED_GPIO_Port, USER_LED_Pin);//Lassú villogással jelezzük a játék végét
		Sys_DelayMs(300);
	}

	if (!HAL_GPIO_ReadPin(LCD_BTN2_GPIO_Port, LCD_BTN2_Pin)) {
		HAL_GPIO_WritePin(USER_LED_GPIO_Port, USER_LED_Pin, RESET);
		start = false;
		LCD_Clear();
		lcdcleared = false;
		Init_Game();
		HAL_UART_Transmit(&huart2, &score, sizeof(char), 10);
	}
}

//Beállítások menüjét megvalósító függvény
void Options_Menu() {
	char str[20];
	int brightness, length;
	int y;
	bool brightnessChanged = false;

	LCD_Clear();
	Sys_DelayMs(100);
	while (HAL_GPIO_ReadPin(BTN_RIGHT_GPIO_Port, BTN_RIGHT_Pin)) {//Addig maradunk a menüben, amíg meg nem nyomjuk az adott gombot
		if (!HAL_GPIO_ReadPin(LCD_BTN2_GPIO_Port, LCD_BTN2_Pin)	//A bal szélső gombbal csökkenthetjük a fényerőt azáltal, hogy a TIM2 CCR1 regiszterébe
		&& TIM2->CCR1 < TIM2->ARR - 1000) {	//kisebb számot írunk (ezáltal csökkentjük a Duty cyclet).
			Sys_DelayMs(100);
			TIM2->CCR1 += 1000;
			brightnessChanged = true;
		}

		else if (!HAL_GPIO_ReadPin(LCD_BTN1_GPIO_Port, LCD_BTN1_Pin)//A jobb szélső gombbal növelhetjük a fényerőt azáltal, hogy a TIM2 CCR1 regiszterébe
		&& TIM2->CCR1 > 500) {//nagyobb számot írunk (ezáltal növelhetjük a Duty cyclet).
			Sys_DelayMs(100);
			TIM2->CCR1 -= 1000;
			brightnessChanged = true;
		}//A Duty cyclet 10%-os léptékben növelhetjük/csökkenthetjük, a szélső értékek 0-100
		 //túllépése hatástalan (kezeljük)
		if (brightnessChanged) {//Ha változtattunk fényerőt akkor törölnünk kell a kijelzőt, hogy megjeleníthessük az új fényerő adatot
			LCD_Clear();
			brightnessChanged = false;
		}

		brightness = TIM2->CCR1 / 100;//Brightness változó tárolja a fényerőt %-ban értve

		if (brightness == 100)
			y = 48;
		else
			y = 54;

		length = snprintf(NULL, 0, "%d", brightness);//Stringgé konvertáljuk a brightnesst, majd kiírjuk a kijelzőre
		snprintf(str, length + 1, "%d", brightness);

		LCD_WriteString("BRIGHTNESS:", 32, 2);
		LCD_WriteString("<", 32, 4);
		LCD_WriteString(">", 90, 4);
		LCD_WriteString(str, y, 4);
		LCD_WriteString("%", 69, 4);
	}
	LCD_Clear();
	Sys_DelayMs(100);
}

//A játék főmenüjét megvalósító függvény
void Game_Menu() {
	if (menu != previousMenu) {	//Ha új menüre lépünk akkor a kurzort oda visszük és frissítjük a kijelzőt
		LCD_Clear();
	}

	if (menu == START) {//Ha a START menün állunk és megnyomunk egy adott gombot, akkor elindul a játék

		LCD_WriteString("START GAME <<", 30, 2);
		LCD_WriteString("OPTIONS", 40, 5);
		previousMenu = START;
		if (!HAL_GPIO_ReadPin(BTN_RIGHT_GPIO_Port, BTN_RIGHT_Pin)) {
			start = true;
		}

	} else if (menu == OPTIONS) {//Ha az OPTIONS menün állunk és megnyomunk egy adott gombot, akkor elindul állíthatjuk a fényerőt
		LCD_WriteString("START GAME ", 30, 2);
		LCD_WriteString("OPTIONS <<", 40, 5);
		previousMenu = OPTIONS;

		if (!HAL_GPIO_ReadPin(BTN_RIGHT_GPIO_Port, BTN_RIGHT_Pin)) {
			Options_Menu();
		}
	}
}

//A játék főciklusa
void Gameloop() {
	if (!start) {
		Game_Menu();
	}

	else {
		while (!gameover) {	//Amíg nem vesztettünk (vagy nyertünk) addig fut a ciklus
			Update_Screen();		//Frissítjük a kijelzőt
			Is_Apple_Eaten();		//Vizsgáljuk, hogy ettünk-e almát

			if (timerflag) {//Ha az időzítő lejár és timerflag = true, akkor megnézzük az aktuális irányt, valamint léptetjük a kígyót ennek megfelelően
				Check_Direction();
				timerflag = false;
			}
		}
		Game_Over();
	}
}
