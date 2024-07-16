#include "main.h"
#include "uart.h"
#include "game.h"
#include "string.h"

extern Snake snake;

UART_HandleTypeDef huart2;
uint8_t rxbuffer;
uint8_t lastreceived;

void Change_Dir() {
	switch (lastreceived) {
	case 'w':
		if (snake.direction != DOWN)
			snake.direction = UP;
		break;

	case 'a':
		if (snake.direction != RIGHT)
			snake.direction = LEFT;
		break;

	case 's':
		if (snake.direction != UP)
			snake.direction = DOWN;
		break;

	case 'd':
		if (snake.direction != LEFT)
			snake.direction = RIGHT;
		break;

	default:
		snake.direction = snake.direction;
		break;
	}
}

void MX_USART2_UART_Init(void) {
	huart2.Instance = USART2;
	huart2.Init.BaudRate = 115200;
	huart2.Init.WordLength = UART_WORDLENGTH_8B;
	huart2.Init.StopBits = UART_STOPBITS_1;
	huart2.Init.Parity = UART_PARITY_NONE;
	huart2.Init.Mode = UART_MODE_TX_RX;
	huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart2.Init.OverSampling = UART_OVERSAMPLING_16;

	if (HAL_UART_Init(&huart2) != HAL_OK) {
		Error_Handler();
	}

	HAL_UART_Receive_IT(&huart2, &rxbuffer, 1);
}

void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart) {
	lastreceived = rxbuffer;
	Change_Dir();
	HAL_UART_Receive_IT(&huart2, &rxbuffer, 1);
}
