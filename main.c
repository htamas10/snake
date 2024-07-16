#include "main.h"
#include "stdbool.h"
#include "lcd.h"
#include "game.h"
#include "uart.h"

TIM_HandleTypeDef htim2;
TIM_HandleTypeDef htim4;
TIM_HandleTypeDef htim5;
extern UART_HandleTypeDef huart2;

extern Snake snake;
extern Menu menu;

volatile int sys_delay = 0;
volatile int millis = 0;
volatile bool timerflag = false; //a játék "ütemezője" ha 'true' akkor lép a kígyó

bool start = false;						//ha 'true' akkor kezdődik a játékciklus

int currentMillis = 0;		//szoftveres pergésmentesítéshez használt változók
int previousMillis = 0;

void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_TIM2_Init(void);
static void MX_TIM4_Init(void);
static void MX_TIM5_Init(void);

void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin) {//külső IT kezelő függvény, amit a négy nyomógombot lefutó éle vált ki
	currentMillis = HAL_GetTick();

	if (GPIO_Pin == BTN_LEFT_Pin) {	//megvárjuk a DEBOUNCE_TIME-ot (szoftveres pergésmentesítés), ez a 'main.h'-ban állítható
		if (start//csak akkor változtatunk irányt, ha nem az aktuális ellentétes irányba akarunk mozogni, illetve ha
		&& snake.direction					//pörög a játékciklus (start = true)
		!= RIGHT && (currentMillis - previousMillis) > DEBOUNCE_TIME)
			snake.direction = LEFT;

	} else if (GPIO_Pin == BTN_RIGHT_Pin) {	//ugyan az a logika, mint az előző esetben stb...
		if (start
				&& snake.direction
						!= LEFT&& (currentMillis - previousMillis) > DEBOUNCE_TIME)
			snake.direction = RIGHT;

	} else if (GPIO_Pin == BTN_UP_Pin) {
		if (start
				&& snake.direction
						!= DOWN&& (currentMillis - previousMillis) > DEBOUNCE_TIME)
			snake.direction = UP;
		menu = START;

	} else if (GPIO_Pin == BTN_DOWN_Pin) {
		if (start
				&& snake.direction
						!= UP&& (currentMillis - previousMillis) > DEBOUNCE_TIME)
			snake.direction = DOWN;
		menu = OPTIONS;
	}

	previousMillis = currentMillis;
}

void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *handle) {
	UNUSED(handle);

	if (handle->Instance == TIM4) {	//TIM4 a ms/us-es várakozást valósítja meg (delayus, delayms)
		if (sys_delay > 0) {
			sys_delay--;
		}
	}

	if (handle->Instance == TIM5) {	//TIM5 a játékórát valósítja meg, melynek periódusideje a kígyó sebessége 'ms'-ben értve
		if (millis == snake.speed) {//kisebb abszolút értékű sebesség valójában nagyobb sebességet jelent a játékban
			millis = 0;
			timerflag = true;
		} else
			millis++;
	}

}

//Mikroszekundumos felbontású késleltetés
void Sys_DelayUs(int us) {
	sys_delay = us;
	while (sys_delay)
		;
}

//Milliszekundumos felbontású késleltetés
void Sys_DelayMs(int ms) {
	sys_delay = ms * 1000;
	while (sys_delay)
		;
}

//LCD, illetve UART inicializáció
void Init_Peripherals() {
	HAL_Init();
	SystemClock_Config();
	MX_GPIO_Init();
	MX_TIM2_Init();
	MX_TIM4_Init();
	MX_TIM5_Init();
	MX_USART2_UART_Init();
}

int main(void) {

	//Perifériák inicializálása, órajel, időzítők, GPIO
	Init_Peripherals();

	//Timerek indítása, ebből 2 IT-t vált ki, 1 pedig a PWM-et valósítja meg az LCD háttérvilágításához
	HAL_TIM_Base_Start_IT(&htim4);
	HAL_TIM_Base_Start_IT(&htim5);
	HAL_TIM_PWM_Start(&htim2, TIM_CHANNEL_1);

	Init_LCD();
	Init_Game();

	//A játék elején a menürendszer fogad minket, itt lehetőség van elindítani a játékot valamint beállítani a fényerőt

	while (1) {
		Gameloop();
	}
}

//Órajel, timerek, gpio inicializációt megvalósító függvények...
void SystemClock_Config(void) {
	RCC_OscInitTypeDef RCC_OscInitStruct = { 0 };
	RCC_ClkInitTypeDef RCC_ClkInitStruct = { 0 };

	__HAL_RCC_PWR_CLK_ENABLE();
	__HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
	RCC_OscInitStruct.PLL.PLLM = 16;
	RCC_OscInitStruct.PLL.PLLN = 336;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV4;
	RCC_OscInitStruct.PLL.PLLQ = 2;
	RCC_OscInitStruct.PLL.PLLR = 2;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK) {
		Error_Handler();
	}

	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK | RCC_CLOCKTYPE_SYSCLK
			| RCC_CLOCKTYPE_PCLK1 | RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLRCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV4;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV2;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK) {
		Error_Handler();
	}
}

static void MX_TIM2_Init(void) {
	TIM_ClockConfigTypeDef sClockSourceConfig = { 0 };
	TIM_MasterConfigTypeDef sMasterConfig = { 0 };
	TIM_OC_InitTypeDef sConfigOC = { 0 };

	htim2.Instance = TIM2;
	htim2.Init.Prescaler = 0;
	htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim2.Init.Period = 10001;
	htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
	if (HAL_TIM_Base_Init(&htim2) != HAL_OK) {
		Error_Handler();
	}
	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK) {
		Error_Handler();
	}
	if (HAL_TIM_PWM_Init(&htim2) != HAL_OK) {
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig)
			!= HAL_OK) {
		Error_Handler();
	}
	sConfigOC.OCMode = TIM_OCMODE_PWM1;
	sConfigOC.Pulse = 5000;
	sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
	sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
	if (HAL_TIM_PWM_ConfigChannel(&htim2, &sConfigOC, TIM_CHANNEL_1)
			!= HAL_OK) {
		Error_Handler();
	}

	HAL_TIM_MspPostInit(&htim2);
}

static void MX_TIM4_Init(void) {
	TIM_ClockConfigTypeDef sClockSourceConfig = { 0 };
	TIM_MasterConfigTypeDef sMasterConfig = { 0 };

	htim4.Instance = TIM4;
	htim4.Init.Prescaler = 0;
	htim4.Init.CounterMode = TIM_COUNTERMODE_DOWN;
	htim4.Init.Period = 84;
	htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_Base_Init(&htim4) != HAL_OK) {
		Error_Handler();
	}
	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if (HAL_TIM_ConfigClockSource(&htim4, &sClockSourceConfig) != HAL_OK) {
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig)
			!= HAL_OK) {
		Error_Handler();
	}
}

static void MX_TIM5_Init(void) {
	TIM_ClockConfigTypeDef sClockSourceConfig = { 0 };
	TIM_MasterConfigTypeDef sMasterConfig = { 0 };

	htim5.Instance = TIM5;
	htim5.Init.Prescaler = 83;
	htim5.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim5.Init.Period = 1000;
	htim5.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim5.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
	if (HAL_TIM_Base_Init(&htim5) != HAL_OK) {
		Error_Handler();
	}
	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if (HAL_TIM_ConfigClockSource(&htim5, &sClockSourceConfig) != HAL_OK) {
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim5, &sMasterConfig)
			!= HAL_OK) {
		Error_Handler();
	}
}

static void MX_GPIO_Init(void) {
	GPIO_InitTypeDef GPIO_InitStruct = { 0 };

	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOH_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();
	__HAL_RCC_GPIOB_CLK_ENABLE();

	HAL_GPIO_WritePin(GPIOA, LD2_Pin | LCD_EN_Pin | USER_LED_Pin,
			GPIO_PIN_RESET);

	HAL_GPIO_WritePin(IC_EN_GPIO_Port, IC_EN_Pin, GPIO_PIN_SET);

	HAL_GPIO_WritePin(GPIOC,
	DB0_Pin | DB1_Pin | DB2_Pin | DB3_Pin | DB4_Pin | LCD_DI_Pin,
			GPIO_PIN_RESET);

	HAL_GPIO_WritePin(GPIOB, DB7_Pin | LCD_RW_Pin | DB5_Pin | DB6_Pin,
			GPIO_PIN_RESET);

	HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_SET);

	HAL_GPIO_WritePin(GPIOC, LCD_CS1_Pin | LCD_CS2_Pin, GPIO_PIN_SET);

	GPIO_InitStruct.Pin = B1_Pin | BTN_RIGHT_Pin | BTN_DOWN_Pin | BTN_UP_Pin
			| BTN_LEFT_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_FALLING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = LCD_BTN2_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(LCD_BTN2_GPIO_Port, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = LD2_Pin | IC_EN_Pin | LCD_EN_Pin | USER_LED_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOA, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = DB0_Pin | DB1_Pin | DB2_Pin | DB3_Pin | DB4_Pin
			| LCD_CS1_Pin | LCD_CS2_Pin | LCD_DI_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOC, &GPIO_InitStruct);

	GPIO_InitStruct.Pin = LCD_BTN1_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_INPUT;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(LCD_BTN1_GPIO_Port, &GPIO_InitStruct);

	GPIO_InitStruct.Pin =
	DB7_Pin | LCD_RW_Pin | LCD_RST_Pin | DB5_Pin | DB6_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GPIOB, &GPIO_InitStruct);

	HAL_NVIC_SetPriority(EXTI0_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(EXTI0_IRQn);

	HAL_NVIC_SetPriority(EXTI1_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(EXTI1_IRQn);

	HAL_NVIC_SetPriority(EXTI2_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(EXTI2_IRQn);

	HAL_NVIC_SetPriority(EXTI3_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(EXTI3_IRQn);
}

void Error_Handler(void) {
	__disable_irq();
	while (1) {
	}
}

#ifdef  USE_FULL_ASSERT

void assert_failed(uint8_t *file, uint32_t line)
{
}
#endif
