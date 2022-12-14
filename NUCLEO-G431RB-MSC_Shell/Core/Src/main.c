/* USER CODE BEGIN Header */
/**
 ******************************************************************************
 * @file           : main.c
 * @brief          : Main program body
 ******************************************************************************
 * @attention
 *
 * Copyright (c) 2022 STMicroelectronics.
 * All rights reserved.
 *
 * This software is licensed under terms that can be found in the LICENSE file
 * in the root directory of this software component.
 * If no LICENSE file comes with this software, it is provided AS-IS.
 *
 ******************************************************************************
 */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <math.h>
#include <stdio.h>
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define UART_TX_BUFFER_SIZE 64
#define UART_RX_BUFFER_SIZE 1
#define CMD_BUFFER_SIZE 64
#define MAX_ARGS 9
//ADC DMA defines
#define ADC_BUFF_SIZE 10
// LF = line feed, saut de ligne
#define ASCII_LF 0x0A
// CR = carriage return, retour chariot
#define ASCII_CR 0x0D
// DEL = delete
#define ASCII_DEL 0x7F
#define TIM1_ARR 5312
//ISO_RESET
#define ISO_RESET_TIME 70
//SPEED TIMERS DEFINES
#define HALF_COUNTER_ENCODER 32767
#define SAMPLING_FREQ_SPEED 100
#define RISES_PER_TURN 4096

//CURRENT SAMPLING PARAMETERS
#define SAMPLING_FREQ_CURRENT 32000

//integral and proportional gains for current loop
#define KpC 0.0342717381517845
#define KiC 25.206814338745
// #define KiC 0.00118870172705228
//integral and proportional gains for speed loop
#define KpS 2.25311945059239
#define KiS 7.88672319650441

#define NOMINAL_CURRENT 12.2
/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
ADC_HandleTypeDef hadc1;
DMA_HandleTypeDef hdma_adc1;

TIM_HandleTypeDef htim1;
TIM_HandleTypeDef htim3;
TIM_HandleTypeDef htim4;
TIM_HandleTypeDef htim8;

UART_HandleTypeDef huart2;

/* USER CODE BEGIN PV */
uint8_t prompt[]="user@Nucleo-STM32G431>>";
uint8_t started[]=
		"\r\n*-----------------------------*"
		"\r\n| Welcome on Nucleo-STM32G431 |"
		"\r\n*-----------------------------*"
		"\r\n";
uint8_t newline[]="\r\n";
uint8_t cmdNotFound[]="Command not found\r\n";
uint8_t helpContent[]="\r\n set \r\n get \r\n pinout \r\n start \r\n stop \r\n dutycycle - value between 0 and 100 \r\n current - value in mAmpere requested \r\n \r\n";
uint8_t pinoutContent[]="\r\n PA2: USART TX \r\n PA3: USART RX \r\n PA5: LED \r\n PA8: TIM1_CH1 \r\n PA9: TIM1_CH2 \r\n PA11: TIM1_CH1N \r\n PA12: TIM1_CH2N \r\n PC13: BUTTON \r\n";
uint8_t startContent[]="\r\n power on \r\n";
uint8_t stopContent[]="\r\n power off \r\n";
uint8_t dutyCycleChoiceContent[]="\r\n duty cycle value chosen \r\n";
uint8_t CurrentChoiceContent[]="\r\n current value chosen \r\n";
int dutycycle; /**< value between 1 and 0 */
int pulse;

uint32_t uartRxReceived;
uint32_t DMA_Received; /**< variable used as a flag. If it is 1 the DMA buffer is full and the current average calculation process can be performed, if it is 0 DMA is not full yet */
uint8_t uartRxBuffer[UART_RX_BUFFER_SIZE];
uint8_t uartTxBuffer[UART_TX_BUFFER_SIZE];
uint16_t ADC_buffer[ADC_BUFF_SIZE];
int i=0; /**< used for ISO_RESET time counter and to sum elements in DMA buffer */

uint16_t Raw_Data_Sum=0; /**< variable in which is stored the sum of all the raw values from the sensor */
float Average_Voltage; /**< raw sensed value on average */
float Converted_Average_Voltage; /**< actual voltage value in output, converted ad follows: Converted_Average_Voltage=(Average_Voltage/4095.0)*3.3 */
float Sensed_Current_Value; /**< average current value in output, converted ad follows: Sensed_Current_Value=12*(Converted_Average_Voltage -2.5); values for the conversion are take by datasheet */
float CurrentValueFloat; /**< value of the current at present time */
float requestedCurrent; /**< value in Ampere */
float errorI[2];/**<array with the values of the current error at previous time instant, errorI[0], and present time instant, errorI[1] */
float alphaI[2];/**<array with the values of the dutycycle value of the integral part of the controller at previous time instant, errorI[0], and present time instant, errorI[1]*/
float alphaP; /**< proportional part of current control*/
float oldCurrent;  /**<value of the current at the previous time instant used for PI controller. Set at the beginning to 0*/

float RotSpeed;
float requestedSpeed; //**< value in rpm*/
float speedP;  /**< proportional part of speed control*/
float errorS[2];/**<array with the values of the speed error at previous time instant, errorI[0], and present time instant, errorI[1] */
float speedI[2];/**<array with the values of the speed value of the integral part of the controller at previous time instant, errorI[0], and present time instant, errorI[1]*/
char Current_Sensed[50];
int CurrentPIcontrollerEnable=0;
int SpeedPIcontrollerEnable=0;
int k=0;  /**< used in the for to print the sensed current value with a delay (only for debug) */

static uint32_t EncoderCounterValue;
char Speed_Sensed[50];
int CounterTaken = 0;
float speed;
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_USART2_UART_Init(void);
static void MX_TIM1_Init(void);
static void MX_ADC1_Init(void);
static void MX_TIM8_Init(void);
static void MX_TIM3_Init(void);
static void MX_TIM4_Init(void);
/* USER CODE BEGIN PFP */

/**
 * @brief ISO_RESET signal function
 * @note   This function is called  when command "start" is written in the terminal
 * ISO_RESET pin is set to high level for at least 20 microseconds and then reset to low
 * using a for cycle to delay the operation
 * @param None
 * @retval None
 */
void ISO_RESET()
{
	HAL_GPIO_WritePin(ISO_RESET_GPIO_Port, ISO_RESET_Pin, GPIO_PIN_SET);
	for(i=0; i<ISO_RESET_TIME; i++)
	{
		//wait for at least 2microSec
	}
	HAL_GPIO_WritePin(ISO_RESET_GPIO_Port, ISO_RESET_Pin, GPIO_PIN_RESET);
}
/**
 * @brief PWM timers start function
 * @note   This function is called  when command "start" is written in the terminal
 * after ISO_RESET command the PWM timers are started to command the power supply and then the motor
 * @param None
 * @retval None
 */
void StartPWM()
{
	//Timers start
	HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_1);
	HAL_TIM_PWM_Start(&htim1, TIM_CHANNEL_2);
	HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_1);
	HAL_TIMEx_PWMN_Start(&htim1, TIM_CHANNEL_2);
}
/**
 * @brief PWM timers stop function
 * @note   This function is called  when command "stop" is written in the terminal
 * PWM timers are stopped and then the motor is stopped
 * @param None
 * @retval None
 */
void StopPWM()
{
	//Timers stop
	HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_1);
	HAL_TIM_PWM_Stop(&htim1, TIM_CHANNEL_2);
	HAL_TIMEx_PWMN_Stop(&htim1, TIM_CHANNEL_1);
	HAL_TIMEx_PWMN_Stop(&htim1, TIM_CHANNEL_2);
}

/**
 * @brief duty cycle function definition
 * @note   This function converts raw voltage data from ADC to current value to be used in the controller
 * @param int
 * @retval None
 */
void DutyCycleFuncDef(int dutycycleIN)
{
	pulse=(dutycycleIN*TIM1_ARR)/100;
	TIM1->CCR1=pulse;
	TIM1->CCR2=TIM1_ARR-pulse;
}
/**
 * @brief current conversion function
 * @note   This function converts raw voltage data from ADC to current value to be used in the controller
 * @param None
 * @retval float
 */
float CurrentConversion()
{
	//HAL_UART_Transmit(&huart2, "DMA current recieved\r\n ", sizeof("DMA current recieved\r\n"), HAL_MAX_DELAY);
	Raw_Data_Sum=0;
	for(i=0;i<10;i++){
		Raw_Data_Sum=Raw_Data_Sum+ADC_buffer[i];
	}
	Average_Voltage=Raw_Data_Sum/ADC_BUFF_SIZE;
	Converted_Average_Voltage=(Average_Voltage/4095.0)*3.3;
	Sensed_Current_Value=12*(Converted_Average_Voltage -2.5); //usually 12 but for this acheur is 2,25 (3x1,5/2)
	sprintf(Current_Sensed, "current sensed is %.3f \r\n", Sensed_Current_Value);
	//for debug - prints current value
	if(k==100000){
		HAL_UART_Transmit(&huart2, Current_Sensed, sizeof(Current_Sensed), HAL_MAX_DELAY);
		//HAL_UART_Transmit(&huart2, ADC_buffer, sizeof(ADC_buffer), HAL_MAX_DELAY);
		k=0;
	}
	return Sensed_Current_Value;
}
/**
 * @brief speed calculation from encoder counter value
 * @note   This function converts encoder counter value in speed in rad/sec
 * @param int
 * @retval float
 */
float SpeedCalculation(uint32_t counter)
{
	speed = 2 * 3.14 * (((float)counter-HALF_COUNTER_ENCODER)/RISES_PER_TURN) * SAMPLING_FREQ_SPEED;
	return speed;
}
/**
 * @brief PI controller for current loop
 * @note   this note takes the value of the current requested by the user and determines the duty cycle. The PI controller gains are determined with simulation and
 * are defined values. The anti wind-up is done as well in the controller. Current values are defined globally and used in the function.
 * the output value is the dutycycle value, between 0 and 100
 * @param float
 * @retval int
 */
int CurrentPI(float CurrentReq)
{
	int alpha;

	//new error calculation
	errorI[1]=CurrentReq-CurrentValueFloat;

	//proportional and integral alpha calculation
	alphaP=KpC*errorI[1];
	alphaI[1]=alphaI[0]+((KiC/(2*SAMPLING_FREQ_CURRENT))*(errorI[1]+errorI[0]));
	alpha= 100*(alphaP+alphaI[1]);

	//error and alpha values storage
	errorI[0]=errorI[1];
	alphaI[0]=alphaI[1];

	//anti wind-up
	if(alpha>100)
	{
		alpha=100;
		alphaI[0]=1;
	}
	else if(alpha<0)
	{
		alpha=0;
		alphaI[0]=0;
	}
	return alpha;

}
/**
 * @brief PI controller for speed loop
 * @note   this note takes the value of the current requested by the user and determines the duty cycle. The PI controller gains are determined with simulation and
 * are defined values. The anti wind-up is done as well in the controller. Current values are defined globally and used in the function.
 * the output value is the current needed for the speed requested
 * @param float
 * @retval int
 */
float SpeedPI(float SpeedReq)
{
	float currentControlled;

	//new error calculation
	errorS[1]=SpeedReq - RotSpeed;

	//proportional and integral alpha calculation
	speedP=KpS*errorS[1];
	speedI[1]=speedI[0]+((KiS/(2*SAMPLING_FREQ_SPEED))*(errorS[1]+errorS[0]));
	currentControlled = (speedP+speedI[1]);

	//error and alpha values storage
	errorS[0]=errorS[1];
	speedI[0]=speedI[1];

	//anti wind-up
	if(currentControlled>NOMINAL_CURRENT)
	{
		currentControlled=NOMINAL_CURRENT;
		errorS[0]=NOMINAL_CURRENT;
	}
	else if(currentControlled<-NOMINAL_CURRENT)
	{
		currentControlled=-NOMINAL_CURRENT;
		errorS[0]=-NOMINAL_CURRENT;
	}
	return currentControlled;

}
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */


/* USER CODE END 0 */

/**
 * @brief  The application entry point.
 * @retval int
 */
int main(void)
{
	/* USER CODE BEGIN 1 */
	char	 	cmdBuffer[CMD_BUFFER_SIZE];
	int 		idx_cmd;
	char* 		argv[MAX_ARGS];
	int		 	argc = 0;
	char*		token;
	int 		newCmdReady = 0;
	//variable initialization
	oldCurrent=0;
	errorI[0]=0;
	alphaI[0]=0.5;
	CurrentValueFloat=0;
	errorS[0]=0;
	speedI[0]=0;
	/* USER CODE END 1 */

	/* MCU Configuration--------------------------------------------------------*/

	/* Reset of all peripherals, Initializes the Flash interface and the Systick. */
	HAL_Init();

	/* USER CODE BEGIN Init */

	/* USER CODE END Init */

	/* Configure the system clock */
	SystemClock_Config();

	/* USER CODE BEGIN SysInit */

	/* USER CODE END SysInit */

	/* Initialize all configured peripherals */
	MX_GPIO_Init();
	MX_DMA_Init();
	MX_USART2_UART_Init();
	MX_TIM1_Init();
	MX_ADC1_Init();
	MX_TIM8_Init();
	MX_TIM3_Init();
	MX_TIM4_Init();
	/* USER CODE BEGIN 2 */
	memset(argv,NULL,MAX_ARGS*sizeof(char*));
	memset(cmdBuffer,NULL,CMD_BUFFER_SIZE*sizeof(char));
	memset(uartRxBuffer,NULL,UART_RX_BUFFER_SIZE*sizeof(char));
	memset(uartTxBuffer,NULL,UART_TX_BUFFER_SIZE*sizeof(char));

	HAL_UART_Receive_IT(&huart2, uartRxBuffer, UART_RX_BUFFER_SIZE);
	HAL_Delay(10);
	HAL_UART_Transmit(&huart2, started, sizeof(started), HAL_MAX_DELAY);
	HAL_UART_Transmit(&huart2, prompt, sizeof(prompt), HAL_MAX_DELAY);

	//ADC starting functions
	HAL_ADCEx_Calibration_Start(&hadc1,ADC_SINGLE_ENDED);
	HAL_ADC_Start_DMA(&hadc1, ADC_buffer, ADC_BUFF_SIZE);
	HAL_TIM_Base_Start(&htim8);

	//Encoder mode TIM3 starting
	HAL_TIM_Encoder_Start_IT(&htim3, TIM_CHANNEL_ALL);
	__HAL_TIM_SET_COUNTER(&htim3, HALF_COUNTER_ENCODER); //set the counter to the half value

	// TIM4 start. Used to measure the time and determine speed
	HAL_TIM_Base_Start_IT(&htim4);

	/* USER CODE END 2 */

	/* Infinite loop */
	/* USER CODE BEGIN WHILE */
	while (1)
	{
		// uartRxReceived is set to 1 when a new character is received on uart 1
		if(uartRxReceived){
			switch(uartRxBuffer[0]){
			// Nouvelle ligne, instruction ?? traiter
			case ASCII_CR:
				HAL_UART_Transmit(&huart2, newline, sizeof(newline), HAL_MAX_DELAY);
				cmdBuffer[idx_cmd] = '\0';
				argc = 0;
				token = strtok(cmdBuffer, " ");
				while(token!=NULL){
					argv[argc++] = token;
					token = strtok(NULL, " ");
				}

				//requested echo transmission to check characters

				HAL_UART_Transmit(&huart2, "\r\n ", sizeof("\r\n "), HAL_MAX_DELAY);
				HAL_UART_Transmit(&huart2, cmdBuffer, sizeof(cmdBuffer), HAL_MAX_DELAY);
				HAL_UART_Transmit(&huart2, "\r\n ", sizeof("\r\n "), HAL_MAX_DELAY);

				idx_cmd = 0;
				newCmdReady = 1;
				break;
				// Suppression du dernier caract??re
			case ASCII_DEL:
				cmdBuffer[idx_cmd--] = '\0';
				HAL_UART_Transmit(&huart2, uartRxBuffer, UART_RX_BUFFER_SIZE, HAL_MAX_DELAY);
				break;
				// Nouveau caract??re
			default:
				cmdBuffer[idx_cmd++] = uartRxBuffer[0];
				HAL_UART_Transmit(&huart2, uartRxBuffer, UART_RX_BUFFER_SIZE, HAL_MAX_DELAY);
			}
			uartRxReceived = 0;
		}

		if(newCmdReady){
			if(strcmp(argv[0],"set")==0){
				if(strcmp(argv[1],"PA5")==0){
					HAL_GPIO_WritePin(GREEN_LED_GPIO_Port, GREEN_LED_Pin, atoi(argv[2]));
					sprintf(uartTxBuffer,"Switch on/off led : %d\r\n",atoi(argv[2]));
					HAL_UART_Transmit(&huart2, uartTxBuffer, 32, HAL_MAX_DELAY);
				}
				else{
					HAL_UART_Transmit(&huart2, cmdNotFound, sizeof(cmdNotFound), HAL_MAX_DELAY);
				}
			}
			else if(strcmp(argv[0],"get")==0)
			{
				HAL_UART_Transmit(&huart2, cmdNotFound, sizeof(cmdNotFound), HAL_MAX_DELAY);
			}

			// help function prints all the available commands
			else if(strcmp(argv[0],"help")==0){
				HAL_UART_Transmit(&huart2, helpContent, sizeof(helpContent), HAL_MAX_DELAY);
			}
			else if(strcmp(argv[0],"pinout")==0){
				HAL_UART_Transmit(&huart2, pinoutContent, sizeof(pinoutContent), HAL_MAX_DELAY);
			}
			else if(strcmp(argv[0],"start")==0){
				HAL_UART_Transmit(&huart2, startContent, sizeof(startContent), HAL_MAX_DELAY);

				//ISO_RESET code
				ISO_RESET();

				//Timers start and dutycycle setting at 50

				dutycycle= 50;
				DutyCycleFuncDef(dutycycle);
				errorI[0]=0;
				alphaI[0]=0.5;
				errorS[0]=0;
				speedI[0]=0;
				StartPWM();


			}
			else if(strcmp(argv[0],"stop")==0){
				HAL_UART_Transmit(&huart2, stopContent, sizeof(stopContent), HAL_MAX_DELAY);
				StopPWM();

				//reset of PWM timers at 50 dutycycle
				dutycycle= 50;
				DutyCycleFuncDef(dutycycle);

				//setting integral error to zero
				errorI[0]=0;
				alphaI[1]=0;
				errorS[0]=0;
				speedI[1]=0;
				CurrentPIcontrollerEnable=0;
				SpeedPIcontrollerEnable=0;

			}
			else if(strcmp(argv[0],"dutycycle")==0){

				HAL_UART_Transmit(&huart2, dutyCycleChoiceContent, sizeof(dutyCycleChoiceContent), HAL_MAX_DELAY);

				dutycycle= atoi(argv[1]);
				DutyCycleFuncDef(dutycycle);

			}
			else if(strcmp(argv[0],"current")==0){

				HAL_UART_Transmit(&huart2, CurrentChoiceContent, sizeof(CurrentChoiceContent), HAL_MAX_DELAY);

				requestedCurrent= atoi(argv[1])/1000.0; // implicit cast from int to float. value from mAmpere to Ampere in float
				CurrentPIcontrollerEnable=1;

			}
			else if(strcmp(argv[0],"speed")==0){

				HAL_UART_Transmit(&huart2, CurrentChoiceContent, sizeof(CurrentChoiceContent), HAL_MAX_DELAY);

				requestedSpeed= atoi(argv[1]); // int value in rpm
				CurrentPIcontrollerEnable=1;
				SpeedPIcontrollerEnable=1;

			}
			else{
				HAL_UART_Transmit(&huart2, cmdNotFound, sizeof(cmdNotFound), HAL_MAX_DELAY);
			}
			HAL_UART_Transmit(&huart2, prompt, sizeof(prompt), HAL_MAX_DELAY);
			newCmdReady = 0;
		}

		if(DMA_Received)
		{
			oldCurrent=CurrentValueFloat;
			CurrentValueFloat=CurrentConversion();

			k++; //value used for debug

			//current controller
			if (CurrentPIcontrollerEnable)
			{
				dutycycle=CurrentPI(requestedCurrent);
				DutyCycleFuncDef(dutycycle);
			}
			DMA_Received=0;
		}
		if (CounterTaken==1)
		{
			RotSpeed = SpeedCalculation(EncoderCounterValue);
			CounterTaken=0;
			/*if(SpeedPIcontrollerEnable=1)
				requestedCurrent=SpeedPI(requestedSpeed);*/
		}
		if(k==54321)
		{
			sprintf(Speed_Sensed, "speed value is %.1f radiant/second \r\n ", RotSpeed);
			HAL_UART_Transmit(&huart2,Speed_Sensed , sizeof(Speed_Sensed), HAL_MAX_DELAY);
		}

		/* USER CODE END WHILE */

		/* USER CODE BEGIN 3 */
	}
	/* USER CODE END 3 */
}

/**
 * @brief System Clock Configuration
 * @retval None
 */
void SystemClock_Config(void)
{
	RCC_OscInitTypeDef RCC_OscInitStruct = {0};
	RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

	/** Configure the main internal regulator output voltage
	 */
	HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST);

	/** Initializes the RCC Oscillators according to the specified parameters
	 * in the RCC_OscInitTypeDef structure.
	 */
	RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
	RCC_OscInitStruct.HSIState = RCC_HSI_ON;
	RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
	RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
	RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
	RCC_OscInitStruct.PLL.PLLM = RCC_PLLM_DIV4;
	RCC_OscInitStruct.PLL.PLLN = 85;
	RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
	RCC_OscInitStruct.PLL.PLLQ = RCC_PLLQ_DIV2;
	RCC_OscInitStruct.PLL.PLLR = RCC_PLLR_DIV2;
	if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
	{
		Error_Handler();
	}

	/** Initializes the CPU, AHB and APB buses clocks
	 */
	RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
			|RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2;
	RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
	RCC_ClkInitStruct.AHBCLKDivider = RCC_SYSCLK_DIV1;
	RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV1;
	RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

	if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_4) != HAL_OK)
	{
		Error_Handler();
	}
}

/**
 * @brief ADC1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_ADC1_Init(void)
{

	/* USER CODE BEGIN ADC1_Init 0 */

	/* USER CODE END ADC1_Init 0 */

	ADC_MultiModeTypeDef multimode = {0};
	ADC_ChannelConfTypeDef sConfig = {0};

	/* USER CODE BEGIN ADC1_Init 1 */

	/* USER CODE END ADC1_Init 1 */

	/** Common config
	 */
	hadc1.Instance = ADC1;
	hadc1.Init.ClockPrescaler = ADC_CLOCK_SYNC_PCLK_DIV4;
	hadc1.Init.Resolution = ADC_RESOLUTION_12B;
	hadc1.Init.DataAlign = ADC_DATAALIGN_RIGHT;
	hadc1.Init.GainCompensation = 0;
	hadc1.Init.ScanConvMode = ADC_SCAN_DISABLE;
	hadc1.Init.EOCSelection = ADC_EOC_SINGLE_CONV;
	hadc1.Init.LowPowerAutoWait = DISABLE;
	hadc1.Init.ContinuousConvMode = DISABLE;
	hadc1.Init.NbrOfConversion = 1;
	hadc1.Init.DiscontinuousConvMode = DISABLE;
	hadc1.Init.ExternalTrigConv = ADC_EXTERNALTRIG_T8_TRGO;
	hadc1.Init.ExternalTrigConvEdge = ADC_EXTERNALTRIGCONVEDGE_RISING;
	hadc1.Init.DMAContinuousRequests = ENABLE;
	hadc1.Init.Overrun = ADC_OVR_DATA_PRESERVED;
	hadc1.Init.OversamplingMode = DISABLE;
	if (HAL_ADC_Init(&hadc1) != HAL_OK)
	{
		Error_Handler();
	}

	/** Configure the ADC multi-mode
	 */
	multimode.Mode = ADC_MODE_INDEPENDENT;
	if (HAL_ADCEx_MultiModeConfigChannel(&hadc1, &multimode) != HAL_OK)
	{
		Error_Handler();
	}

	/** Configure Regular Channel
	 */
	sConfig.Channel = ADC_CHANNEL_1;
	sConfig.Rank = ADC_REGULAR_RANK_1;
	sConfig.SamplingTime = ADC_SAMPLETIME_2CYCLES_5;
	sConfig.SingleDiff = ADC_SINGLE_ENDED;
	sConfig.OffsetNumber = ADC_OFFSET_NONE;
	sConfig.Offset = 0;
	if (HAL_ADC_ConfigChannel(&hadc1, &sConfig) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN ADC1_Init 2 */

	/* USER CODE END ADC1_Init 2 */

}

/**
 * @brief TIM1 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM1_Init(void)
{

	/* USER CODE BEGIN TIM1_Init 0 */

	/* USER CODE END TIM1_Init 0 */

	TIM_ClockConfigTypeDef sClockSourceConfig = {0};
	TIM_MasterConfigTypeDef sMasterConfig = {0};
	TIM_OC_InitTypeDef sConfigOC = {0};
	TIM_BreakDeadTimeConfigTypeDef sBreakDeadTimeConfig = {0};

	/* USER CODE BEGIN TIM1_Init 1 */

	/* USER CODE END TIM1_Init 1 */
	htim1.Instance = TIM1;
	htim1.Init.Prescaler = 0;
	htim1.Init.CounterMode = TIM_COUNTERMODE_CENTERALIGNED1;
	htim1.Init.Period = 5312;
	htim1.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim1.Init.RepetitionCounter = 0;
	htim1.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_ENABLE;
	if (HAL_TIM_Base_Init(&htim1) != HAL_OK)
	{
		Error_Handler();
	}
	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if (HAL_TIM_ConfigClockSource(&htim1, &sClockSourceConfig) != HAL_OK)
	{
		Error_Handler();
	}
	if (HAL_TIM_PWM_Init(&htim1) != HAL_OK)
	{
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim1, &sMasterConfig) != HAL_OK)
	{
		Error_Handler();
	}
	sConfigOC.OCMode = TIM_OCMODE_PWM1;
	sConfigOC.Pulse = 1328;
	sConfigOC.OCPolarity = TIM_OCPOLARITY_HIGH;
	sConfigOC.OCNPolarity = TIM_OCNPOLARITY_HIGH;
	sConfigOC.OCFastMode = TIM_OCFAST_DISABLE;
	sConfigOC.OCIdleState = TIM_OCIDLESTATE_RESET;
	sConfigOC.OCNIdleState = TIM_OCNIDLESTATE_RESET;
	if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_1) != HAL_OK)
	{
		Error_Handler();
	}
	sConfigOC.Pulse = 3986;
	if (HAL_TIM_PWM_ConfigChannel(&htim1, &sConfigOC, TIM_CHANNEL_2) != HAL_OK)
	{
		Error_Handler();
	}
	sBreakDeadTimeConfig.OffStateRunMode = TIM_OSSR_DISABLE;
	sBreakDeadTimeConfig.OffStateIDLEMode = TIM_OSSI_DISABLE;
	sBreakDeadTimeConfig.LockLevel = TIM_LOCKLEVEL_OFF;
	sBreakDeadTimeConfig.DeadTime = 203;
	sBreakDeadTimeConfig.BreakState = TIM_BREAK_DISABLE;
	sBreakDeadTimeConfig.BreakPolarity = TIM_BREAKPOLARITY_HIGH;
	sBreakDeadTimeConfig.BreakFilter = 0;
	sBreakDeadTimeConfig.BreakAFMode = TIM_BREAK_AFMODE_INPUT;
	sBreakDeadTimeConfig.Break2State = TIM_BREAK2_DISABLE;
	sBreakDeadTimeConfig.Break2Polarity = TIM_BREAK2POLARITY_HIGH;
	sBreakDeadTimeConfig.Break2Filter = 0;
	sBreakDeadTimeConfig.Break2AFMode = TIM_BREAK_AFMODE_INPUT;
	sBreakDeadTimeConfig.AutomaticOutput = TIM_AUTOMATICOUTPUT_DISABLE;
	if (HAL_TIMEx_ConfigBreakDeadTime(&htim1, &sBreakDeadTimeConfig) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN TIM1_Init 2 */

	/* USER CODE END TIM1_Init 2 */
	HAL_TIM_MspPostInit(&htim1);

}

/**
 * @brief TIM3 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM3_Init(void)
{

	/* USER CODE BEGIN TIM3_Init 0 */

	/* USER CODE END TIM3_Init 0 */

	TIM_Encoder_InitTypeDef sConfig = {0};
	TIM_MasterConfigTypeDef sMasterConfig = {0};

	/* USER CODE BEGIN TIM3_Init 1 */

	/* USER CODE END TIM3_Init 1 */
	htim3.Instance = TIM3;
	htim3.Init.Prescaler = 0;
	htim3.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim3.Init.Period = 65535;
	htim3.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim3.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	sConfig.EncoderMode = TIM_ENCODERMODE_TI12;
	sConfig.IC1Polarity = TIM_ICPOLARITY_RISING;
	sConfig.IC1Selection = TIM_ICSELECTION_DIRECTTI;
	sConfig.IC1Prescaler = TIM_ICPSC_DIV1;
	sConfig.IC1Filter = 0;
	sConfig.IC2Polarity = TIM_ICPOLARITY_RISING;
	sConfig.IC2Selection = TIM_ICSELECTION_DIRECTTI;
	sConfig.IC2Prescaler = TIM_ICPSC_DIV1;
	sConfig.IC2Filter = 0;
	if (HAL_TIM_Encoder_Init(&htim3, &sConfig) != HAL_OK)
	{
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim3, &sMasterConfig) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN TIM3_Init 2 */

	/* USER CODE END TIM3_Init 2 */

}

/**
 * @brief TIM4 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM4_Init(void)
{

	/* USER CODE BEGIN TIM4_Init 0 */

	/* USER CODE END TIM4_Init 0 */

	TIM_ClockConfigTypeDef sClockSourceConfig = {0};
	TIM_MasterConfigTypeDef sMasterConfig = {0};

	/* USER CODE BEGIN TIM4_Init 1 */

	/* USER CODE END TIM4_Init 1 */
	htim4.Instance = TIM4;
	htim4.Init.Prescaler = 17000-1;
	htim4.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim4.Init.Period = 99;
	htim4.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim4.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_Base_Init(&htim4) != HAL_OK)
	{
		Error_Handler();
	}
	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if (HAL_TIM_ConfigClockSource(&htim4, &sClockSourceConfig) != HAL_OK)
	{
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim4, &sMasterConfig) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN TIM4_Init 2 */

	/* USER CODE END TIM4_Init 2 */

}

/**
 * @brief TIM8 Initialization Function
 * @param None
 * @retval None
 */
static void MX_TIM8_Init(void)
{

	/* USER CODE BEGIN TIM8_Init 0 */

	/* USER CODE END TIM8_Init 0 */

	TIM_ClockConfigTypeDef sClockSourceConfig = {0};
	TIM_MasterConfigTypeDef sMasterConfig = {0};

	/* USER CODE BEGIN TIM8_Init 1 */

	/* USER CODE END TIM8_Init 1 */
	htim8.Instance = TIM8;
	htim8.Init.Prescaler = 0;
	htim8.Init.CounterMode = TIM_COUNTERMODE_UP;
	htim8.Init.Period = 530;
	htim8.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
	htim8.Init.RepetitionCounter = 0;
	htim8.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
	if (HAL_TIM_Base_Init(&htim8) != HAL_OK)
	{
		Error_Handler();
	}
	sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
	if (HAL_TIM_ConfigClockSource(&htim8, &sClockSourceConfig) != HAL_OK)
	{
		Error_Handler();
	}
	sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
	sMasterConfig.MasterOutputTrigger2 = TIM_TRGO2_RESET;
	sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_ENABLE;
	if (HAL_TIMEx_MasterConfigSynchronization(&htim8, &sMasterConfig) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN TIM8_Init 2 */

	/* USER CODE END TIM8_Init 2 */

}

/**
 * @brief USART2 Initialization Function
 * @param None
 * @retval None
 */
static void MX_USART2_UART_Init(void)
{

	/* USER CODE BEGIN USART2_Init 0 */

	/* USER CODE END USART2_Init 0 */

	/* USER CODE BEGIN USART2_Init 1 */

	/* USER CODE END USART2_Init 1 */
	huart2.Instance = USART2;
	huart2.Init.BaudRate = 115200;
	huart2.Init.WordLength = UART_WORDLENGTH_8B;
	huart2.Init.StopBits = UART_STOPBITS_1;
	huart2.Init.Parity = UART_PARITY_NONE;
	huart2.Init.Mode = UART_MODE_TX_RX;
	huart2.Init.HwFlowCtl = UART_HWCONTROL_NONE;
	huart2.Init.OverSampling = UART_OVERSAMPLING_16;
	huart2.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
	huart2.Init.ClockPrescaler = UART_PRESCALER_DIV1;
	huart2.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
	if (HAL_UART_Init(&huart2) != HAL_OK)
	{
		Error_Handler();
	}
	if (HAL_UARTEx_SetTxFifoThreshold(&huart2, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
	{
		Error_Handler();
	}
	if (HAL_UARTEx_SetRxFifoThreshold(&huart2, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
	{
		Error_Handler();
	}
	if (HAL_UARTEx_DisableFifoMode(&huart2) != HAL_OK)
	{
		Error_Handler();
	}
	/* USER CODE BEGIN USART2_Init 2 */

	/* USER CODE END USART2_Init 2 */

}

/**
 * Enable DMA controller clock
 */
static void MX_DMA_Init(void)
{

	/* DMA controller clock enable */
	__HAL_RCC_DMAMUX1_CLK_ENABLE();
	__HAL_RCC_DMA1_CLK_ENABLE();

	/* DMA interrupt init */
	/* DMA1_Channel1_IRQn interrupt configuration */
	HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 0, 0);
	HAL_NVIC_EnableIRQ(DMA1_Channel1_IRQn);

}

/**
 * @brief GPIO Initialization Function
 * @param None
 * @retval None
 */
static void MX_GPIO_Init(void)
{
	GPIO_InitTypeDef GPIO_InitStruct = {0};

	/* GPIO Ports Clock Enable */
	__HAL_RCC_GPIOC_CLK_ENABLE();
	__HAL_RCC_GPIOF_CLK_ENABLE();
	__HAL_RCC_GPIOA_CLK_ENABLE();

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(ISO_RESET_GPIO_Port, ISO_RESET_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin Output Level */
	HAL_GPIO_WritePin(GREEN_LED_GPIO_Port, GREEN_LED_Pin, GPIO_PIN_RESET);

	/*Configure GPIO pin : BLUE_BUTTON_Pin */
	GPIO_InitStruct.Pin = BLUE_BUTTON_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	HAL_GPIO_Init(BLUE_BUTTON_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : ISO_RESET_Pin */
	GPIO_InitStruct.Pin = ISO_RESET_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(ISO_RESET_GPIO_Port, &GPIO_InitStruct);

	/*Configure GPIO pin : GREEN_LED_Pin */
	GPIO_InitStruct.Pin = GREEN_LED_Pin;
	GPIO_InitStruct.Mode = GPIO_MODE_OUTPUT_PP;
	GPIO_InitStruct.Pull = GPIO_NOPULL;
	GPIO_InitStruct.Speed = GPIO_SPEED_FREQ_LOW;
	HAL_GPIO_Init(GREEN_LED_GPIO_Port, &GPIO_InitStruct);

}

/* USER CODE BEGIN 4 */

void HAL_UART_RxCpltCallback (UART_HandleTypeDef * huart){
	uartRxReceived=1;
	HAL_UART_Receive_IT(&huart2, uartRxBuffer, UART_RX_BUFFER_SIZE);

}

/**
 * @brief  ADC callback function
 * @note   This function is called  when the DMA buffer of ADC value is full. The DMA_Received variable is set to 1 to start data conversion in the main
 * @param  ADC handler
 * @retval None
 */
void HAL_ADC_ConvCpltCallback(ADC_HandleTypeDef*hadc){
	//HAL_UART_Transmit(&huart2, "DMA callback entered \r\n", sizeof("DMA callback entered \r\n"), HAL_MAX_DELAY);
	DMA_Received=1;
}
/**
 * @brief  TIM4 callback function
 * @note   This function is called with a fixed frequency determined by the timer parameters to calculate the speed, using encoder counters values and frequency
 * @param  None
 * @retval None
 */

/* USER CODE END 4 */

/**
 * @brief  Period elapsed callback in non blocking mode
 * @note   This function is called  when TIM6 interrupt took place, inside
 * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
 * a global variable "uwTick" used as application time base.
 * @param  htim : TIM handle
 * @retval None
 */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
	/* USER CODE BEGIN Callback 0 */

	/* USER CODE END Callback 0 */
	if (htim->Instance == TIM6) {
		HAL_IncTick();
	}
	/* USER CODE BEGIN Callback 1 */
	if (htim->Instance == TIM4)
	{
		EncoderCounterValue=__HAL_TIM_GET_COUNTER(&htim3);
		__HAL_TIM_SET_COUNTER(&htim3, HALF_COUNTER_ENCODER);
		CounterTaken = 1;
	}
	/* USER CODE END Callback 1 */
}

/**
 * @brief  This function is executed in case of error occurrence.
 * @retval None
 */
void Error_Handler(void)
{
	/* USER CODE BEGIN Error_Handler_Debug */
	/* User can add his own implementation to report the HAL error return state */
	__disable_irq();
	while (1)
	{
	}
	/* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
 * @brief  Reports the name of the source file and the source line number
 *         where the assert_param error has occurred.
 * @param  file: pointer to the source file name
 * @param  line: assert_param error line source number
 * @retval None
 */
void assert_failed(uint8_t *file, uint32_t line)
{
	/* USER CODE BEGIN 6 */
	/* User can add his own implementation to report the file name and line number,
     ex: printf("Wrong parameters value: file %s on line %d\r\n", file, line) */
	/* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
