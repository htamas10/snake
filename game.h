#ifndef INC_GAME_H_
#define INC_GAME_H_

#define MAX_LENGTH 512

typedef enum Direction {
	LEFT, UP, RIGHT, DOWN
} Direction;

typedef enum Menu {
	START, OPTIONS
} Menu;

typedef struct Point {
	char x;
	char y;
} Point;

typedef struct Snake {
	int length;
	int speed;
	Direction direction;
	Point positions[MAX_LENGTH];
} Snake;

void Init_Snake();
void Init_Game();
void Update_Screen();
void Create_Pagemask();
void Gameloop();
void Clear_Gameboard();
void Move_Snake(int deltaX, int deltaY);
void Check_Direction();
void Increase_Snake();
void Spawn_Apple();
void Is_Apple_Eaten();
void Game_Over();
void Game_Menu();
void Options_Menu();

#endif
