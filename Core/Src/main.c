/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file           : main.c
  * @brief          : Main program body
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2026 STMicroelectronics.
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
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "audio.h"
#include "stm32l4s5i_iot01_accelero.h"
#include "tetris.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */
typedef struct {
  uint32_t held_move;
  uint32_t last_move_ms;
  uint32_t repeat_delay_ms;
} TiltRepeat;

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
#define INPUT_ROTATE 1
#define INPUT_LEFT 2
#define INPUT_RIGHT 3
#define INPUT_DOWN 4

#define ACCELERO_POLL_MS 200
#define ACCELERO_CALIBRATION_SAMPLES 10
// X is lateral; change the axis or sign to match the board's held orientation.
#define ACCELERO_LATERAL_AXIS 0
#define ACCELERO_AXIS_SIGN 1
// Y is perpendicular to lateral X; negative tilt requests soft drop.
#define ACCELERO_DROP_AXIS 1
#define ACCELERO_DROP_SIGN -1
#define ACCELERO_DEAD_ZONE_MG 150
#define ACCELERO_RELEASE_ZONE_MG 100
#define INPUT_INITIAL_REPEAT_MS 1000
#define INPUT_REPEAT_MS 750
#define DRAW_READY 1
#define AUDIO_LINE_CLEAR 1
#define AUDIO_GAME_OVER 2

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
DAC_HandleTypeDef hdac1;
DMA_HandleTypeDef hdma_dac1_ch1;

I2C_HandleTypeDef hi2c2;

TIM_HandleTypeDef htim2;

UART_HandleTypeDef huart1;

osThreadId gameStateTaskHandle;
osThreadId playAudioTaskHandle;
osThreadId drawGridTaskHandle;
osThreadId pollAcceleroTasHandle;
osMessageQId inputQueueHandle;
osMessageQId audioQueueHandle;
osMutexId gameStateMutexHandle;
/* USER CODE BEGIN PV */
static const uint32_t GAME_TICK_MS = 1000U;
static const uint32_t GAME_TICK_MIN_MS = 200U;
static const uint32_t GAME_TICK_STEP_MS = 100U;
static const uint32_t SCORE_PER_SPEEDUP = 1000U;
static GameState game_state;

/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MX_GPIO_Init(void);
static void MX_DMA_Init(void);
static void MX_DAC1_Init(void);
static void MX_TIM2_Init(void);
static void MX_USART1_UART_Init(void);
static void MX_I2C2_Init(void);
void StartGameStateTask(void const * argument);
void StartPlayAudioTask(void const * argument);
void StartDrawGridTask(void const * argument);
void StartPollAcceleroTask(void const * argument);

/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
static void queue_tilt_move(TiltRepeat *repeat, uint32_t move, uint32_t now)
{
  if (move != repeat->held_move) {
    repeat->held_move = move;
    repeat->repeat_delay_ms = INPUT_INITIAL_REPEAT_MS;
    repeat->last_move_ms = now;
    if (move) {
      (void)osMessagePut(inputQueueHandle, move, 0);
    }
  } else if (move && (uint32_t)(now - repeat->last_move_ms) >= repeat->repeat_delay_ms) {
    (void)osMessagePut(inputQueueHandle, move, 0);
    repeat->last_move_ms = now;
    repeat->repeat_delay_ms = INPUT_REPEAT_MS;
  }
}

/* USER CODE END 0 */

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */

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
  MX_DAC1_Init();
  MX_TIM2_Init();
  MX_USART1_UART_Init();
  MX_I2C2_Init();
  /* USER CODE BEGIN 2 */
  if (BSP_ACCELERO_Init() != ACCELERO_OK) {
    Error_Handler();
  }
  if (audio_init() != 0) {
      Error_Handler();
  }
  if (init_game_state(&game_state) != 0) {
     Error_Handler();
   }

  /* USER CODE END 2 */

  /* Create the mutex(es) */
  /* definition and creation of gameStateMutex */
  osMutexDef(gameStateMutex);
  gameStateMutexHandle = osMutexCreate(osMutex(gameStateMutex));

  /* USER CODE BEGIN RTOS_MUTEX */
  if (gameStateMutexHandle == NULL) {
    Error_Handler();
  }
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* Create the queue(s) */
  /* definition and creation of inputQueue */
  osMessageQDef(inputQueue, 16, uint32_t);
  inputQueueHandle = osMessageCreate(osMessageQ(inputQueue), NULL);

  /* definition and creation of audioQueue */
  osMessageQDef(audioQueue, 8, uint32_t);
  audioQueueHandle = osMessageCreate(osMessageQ(audioQueue), NULL);

  /* USER CODE BEGIN RTOS_QUEUES */
  if (inputQueueHandle == NULL || audioQueueHandle == NULL) {
    Error_Handler();
  }
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* definition and creation of gameStateTask */
  osThreadDef(gameStateTask, StartGameStateTask, osPriorityAboveNormal, 0, 128);
  gameStateTaskHandle = osThreadCreate(osThread(gameStateTask), NULL);

  /* definition and creation of playAudioTask */
  osThreadDef(playAudioTask, StartPlayAudioTask, osPriorityBelowNormal, 0, 128);
  playAudioTaskHandle = osThreadCreate(osThread(playAudioTask), NULL);

  /* definition and creation of drawGridTask */
  osThreadDef(drawGridTask, StartDrawGridTask, osPriorityNormal, 0, 128);
  drawGridTaskHandle = osThreadCreate(osThread(drawGridTask), NULL);

  /* definition and creation of pollAcceleroTas */
  osThreadDef(pollAcceleroTas, StartPollAcceleroTask, osPriorityNormal, 0, 128);
  pollAcceleroTasHandle = osThreadCreate(osThread(pollAcceleroTas), NULL);

  /* USER CODE BEGIN RTOS_THREADS */
  if (gameStateTaskHandle == NULL || playAudioTaskHandle == NULL
      || drawGridTaskHandle == NULL || pollAcceleroTasHandle == NULL) {
    Error_Handler();
  }
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* Start scheduler */
  osKernelStart();

  /* We should never get here as control is now taken by the scheduler */

  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
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
  if (HAL_PWREx_ControlVoltageScaling(PWR_REGULATOR_VOLTAGE_SCALE1_BOOST) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_MSI;
  RCC_OscInitStruct.MSIState = RCC_MSI_ON;
  RCC_OscInitStruct.MSICalibrationValue = 0;
  RCC_OscInitStruct.MSIClockRange = RCC_MSIRANGE_6;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_MSI;
  RCC_OscInitStruct.PLL.PLLM = 1;
  RCC_OscInitStruct.PLL.PLLN = 60;
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

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_5) != HAL_OK)
  {
    Error_Handler();
  }
}

/**
  * @brief DAC1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_DAC1_Init(void)
{

  /* USER CODE BEGIN DAC1_Init 0 */

  /* USER CODE END DAC1_Init 0 */

  DAC_ChannelConfTypeDef sConfig = {0};

  /* USER CODE BEGIN DAC1_Init 1 */

  /* USER CODE END DAC1_Init 1 */

  /** DAC Initialization
  */
  hdac1.Instance = DAC1;
  if (HAL_DAC_Init(&hdac1) != HAL_OK)
  {
    Error_Handler();
  }

  /** DAC channel OUT1 config
  */
  sConfig.DAC_SampleAndHold = DAC_SAMPLEANDHOLD_DISABLE;
  sConfig.DAC_Trigger = DAC_TRIGGER_T2_TRGO;
  sConfig.DAC_HighFrequency = DAC_HIGH_FREQUENCY_INTERFACE_MODE_ABOVE_80MHZ;
  sConfig.DAC_OutputBuffer = DAC_OUTPUTBUFFER_ENABLE;
  sConfig.DAC_ConnectOnChipPeripheral = DAC_CHIPCONNECT_DISABLE;
  sConfig.DAC_UserTrimming = DAC_TRIMMING_FACTORY;
  if (HAL_DAC_ConfigChannel(&hdac1, &sConfig, DAC_CHANNEL_1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN DAC1_Init 2 */

  /* USER CODE END DAC1_Init 2 */

}

/**
  * @brief I2C2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_I2C2_Init(void)
{

  /* USER CODE BEGIN I2C2_Init 0 */

  /* USER CODE END I2C2_Init 0 */

  /* USER CODE BEGIN I2C2_Init 1 */

  /* USER CODE END I2C2_Init 1 */
  hi2c2.Instance = I2C2;
  hi2c2.Init.Timing = 0x30A175AB;
  hi2c2.Init.OwnAddress1 = 0;
  hi2c2.Init.AddressingMode = I2C_ADDRESSINGMODE_7BIT;
  hi2c2.Init.DualAddressMode = I2C_DUALADDRESS_DISABLE;
  hi2c2.Init.OwnAddress2 = 0;
  hi2c2.Init.OwnAddress2Masks = I2C_OA2_NOMASK;
  hi2c2.Init.GeneralCallMode = I2C_GENERALCALL_DISABLE;
  hi2c2.Init.NoStretchMode = I2C_NOSTRETCH_DISABLE;
  if (HAL_I2C_Init(&hi2c2) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Analogue filter
  */
  if (HAL_I2CEx_ConfigAnalogFilter(&hi2c2, I2C_ANALOGFILTER_ENABLE) != HAL_OK)
  {
    Error_Handler();
  }

  /** Configure Digital filter
  */
  if (HAL_I2CEx_ConfigDigitalFilter(&hi2c2, 0) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN I2C2_Init 2 */

  /* USER CODE END I2C2_Init 2 */

}

/**
  * @brief TIM2 Initialization Function
  * @param None
  * @retval None
  */
static void MX_TIM2_Init(void)
{

  /* USER CODE BEGIN TIM2_Init 0 */

  /* USER CODE END TIM2_Init 0 */

  TIM_ClockConfigTypeDef sClockSourceConfig = {0};
  TIM_MasterConfigTypeDef sMasterConfig = {0};

  /* USER CODE BEGIN TIM2_Init 1 */

  /* USER CODE END TIM2_Init 1 */
  htim2.Instance = TIM2;
  htim2.Init.Prescaler = 0;
  htim2.Init.CounterMode = TIM_COUNTERMODE_UP;
  htim2.Init.Period = 2720;
  htim2.Init.ClockDivision = TIM_CLOCKDIVISION_DIV1;
  htim2.Init.AutoReloadPreload = TIM_AUTORELOAD_PRELOAD_DISABLE;
  if (HAL_TIM_Base_Init(&htim2) != HAL_OK)
  {
    Error_Handler();
  }
  sClockSourceConfig.ClockSource = TIM_CLOCKSOURCE_INTERNAL;
  if (HAL_TIM_ConfigClockSource(&htim2, &sClockSourceConfig) != HAL_OK)
  {
    Error_Handler();
  }
  sMasterConfig.MasterOutputTrigger = TIM_TRGO_UPDATE;
  sMasterConfig.MasterSlaveMode = TIM_MASTERSLAVEMODE_DISABLE;
  if (HAL_TIMEx_MasterConfigSynchronization(&htim2, &sMasterConfig) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN TIM2_Init 2 */

  /* USER CODE END TIM2_Init 2 */

}

/**
  * @brief USART1 Initialization Function
  * @param None
  * @retval None
  */
static void MX_USART1_UART_Init(void)
{

  /* USER CODE BEGIN USART1_Init 0 */

  /* USER CODE END USART1_Init 0 */

  /* USER CODE BEGIN USART1_Init 1 */

  /* USER CODE END USART1_Init 1 */
  huart1.Instance = USART1;
  huart1.Init.BaudRate = 115200;
  huart1.Init.WordLength = UART_WORDLENGTH_8B;
  huart1.Init.StopBits = UART_STOPBITS_1;
  huart1.Init.Parity = UART_PARITY_NONE;
  huart1.Init.Mode = UART_MODE_TX_RX;
  huart1.Init.HwFlowCtl = UART_HWCONTROL_NONE;
  huart1.Init.OverSampling = UART_OVERSAMPLING_16;
  huart1.Init.OneBitSampling = UART_ONE_BIT_SAMPLE_DISABLE;
  huart1.Init.ClockPrescaler = UART_PRESCALER_DIV1;
  huart1.AdvancedInit.AdvFeatureInit = UART_ADVFEATURE_NO_INIT;
  if (HAL_UART_Init(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetTxFifoThreshold(&huart1, UART_TXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_SetRxFifoThreshold(&huart1, UART_RXFIFO_THRESHOLD_1_8) != HAL_OK)
  {
    Error_Handler();
  }
  if (HAL_UARTEx_DisableFifoMode(&huart1) != HAL_OK)
  {
    Error_Handler();
  }
  /* USER CODE BEGIN USART1_Init 2 */

  /* USER CODE END USART1_Init 2 */

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
  HAL_NVIC_SetPriority(DMA1_Channel1_IRQn, 5, 0);
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
  /* USER CODE BEGIN MX_GPIO_Init_1 */

  /* USER CODE END MX_GPIO_Init_1 */

  /* GPIO Ports Clock Enable */
  __HAL_RCC_GPIOC_CLK_ENABLE();
  __HAL_RCC_GPIOA_CLK_ENABLE();
  __HAL_RCC_GPIOB_CLK_ENABLE();

  /*Configure GPIO pin : BTN_Pin */
  GPIO_InitStruct.Pin = BTN_Pin;
  GPIO_InitStruct.Mode = GPIO_MODE_IT_RISING;
  GPIO_InitStruct.Pull = GPIO_NOPULL;
  HAL_GPIO_Init(BTN_GPIO_Port, &GPIO_InitStruct);

  /* EXTI interrupt init*/
  HAL_NVIC_SetPriority(EXTI15_10_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(EXTI15_10_IRQn);

  /* USER CODE BEGIN MX_GPIO_Init_2 */

  /* USER CODE END MX_GPIO_Init_2 */
}

/* USER CODE BEGIN 4 */
void HAL_GPIO_EXTI_Callback(uint16_t GPIO_Pin)
{
  static uint32_t last_press_ms = 0;
  static uint8_t button_seen = 0;

  if (GPIO_Pin != BTN_Pin || inputQueueHandle == NULL || !osKernelRunning()) {
    return;
  }

  uint32_t now = HAL_GetTick();
  if (!button_seen || (uint32_t)(now - last_press_ms) >= 50U) {
    last_press_ms = now;
    button_seen = 1;
    // ISR-safe, nonblocking: ignore the press if the queue is full.
    (void)osMessagePut(inputQueueHandle, INPUT_ROTATE, 0);
  }
}

/* USER CODE END 4 */

/* USER CODE BEGIN Header_StartGameStateTask */
/**
  * @brief  Function implementing the gameStateTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartGameStateTask */
void StartGameStateTask(void const * argument)
{
  /* USER CODE BEGIN 5 */
  uint32_t last_update_ms = HAL_GetTick();
  uint8_t redraw = 1;
  uint32_t pending_audio = 0;

  /* Infinite loop */
  for(;;)
  {
    osDelay(10);

    if (osMutexWait(gameStateMutexHandle, osWaitForever) != osOK) {
      Error_Handler();
    }

    const uint32_t previous_lines = game_state.lines_cleared;
    const bool was_game_over = game_state.game_over;

    // apply queued input in this task so interrupts never modify the board
    osEvent event = osMessageGet(inputQueueHandle, 0);
    while (event.status == osEventMessage) {
      int status = 0;
      switch (event.value.v) {
        case INPUT_ROTATE: status = rotate_piece(&game_state); break;
        case INPUT_LEFT: status = move_piece(&game_state, -1); break;
        case INPUT_RIGHT: status = move_piece(&game_state, 1); break;
        case INPUT_DOWN: status = update_game_state(&game_state); break;
        default: break;
      }
      if (status == -1) {
        Error_Handler();
      }
      redraw = 1;
      event = osMessageGet(inputQueueHandle, 0);
    }

    // speed up game tick w score
    // clamp at min of 200 ms
    const uint32_t speedup_ms = (game_state.score / SCORE_PER_SPEEDUP) * GAME_TICK_STEP_MS;
    const uint32_t tick_ms = speedup_ms >= GAME_TICK_MS - GAME_TICK_MIN_MS
        ? GAME_TICK_MIN_MS : GAME_TICK_MS - speedup_ms;
    uint32_t now = HAL_GetTick();
    if ((uint32_t)(now - last_update_ms) >= tick_ms) {
      last_update_ms = now;
      if (update_game_state(&game_state) == -1) {
        Error_Handler();
      }
      redraw = 1;
    }

    // both gravity and queued soft drops can lock pieces and trigger audio.
    if (game_state.lines_cleared != previous_lines) {
      pending_audio |= AUDIO_LINE_CLEAR;
    }
    if (!was_game_over && game_state.game_over) {
      pending_audio |= AUDIO_GAME_OVER;
    }

    if (osMutexRelease(gameStateMutexHandle) != osOK) {
      Error_Handler();
    }

    // Send outside the state mutex; retry next loop if the queue is full.
    if (pending_audio && osMessagePut(audioQueueHandle, pending_audio, 0) == osOK) {
      pending_audio = 0;
    }

    if (redraw) {
      // notify after unlocking -> repeated signals coalesce into one redraw
      if (osSignalSet(drawGridTaskHandle, DRAW_READY) < 0) {
        Error_Handler();
      }
      redraw = 0;
    }
  }
  /* USER CODE END 5 */
}

/* USER CODE BEGIN Header_StartPlayAudioTask */
/**
* @brief Function implementing the playAudioTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartPlayAudioTask */
void StartPlayAudioTask(void const * argument)
{
  /* USER CODE BEGIN StartPlayAudioTask */
  for(;;)
  {
    osEvent event = osMessageGet(audioQueueHandle, osWaitForever);
    if (event.status != osEventMessage) {
      Error_Handler();
    }
    if ((event.value.v & AUDIO_LINE_CLEAR) && play_tetris_chime() != 0) {
      Error_Handler();
    }
    if ((event.value.v & AUDIO_GAME_OVER) && play_game_over_chime() != 0) {
      Error_Handler();
    }
  }
  /* USER CODE END StartPlayAudioTask */
}

/* USER CODE BEGIN Header_StartDrawGridTask */
/**
* @brief Function implementing the drawGridTask thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartDrawGridTask */
void StartDrawGridTask(void const * argument)
{
  /* USER CODE BEGIN StartDrawGridTask */
  // Keep the full snapshot off this task's small stack.
  static GameState snapshot;

  for(;;)
  {
    osEvent event = osSignalWait(DRAW_READY, osWaitForever);
    if (event.status != osEventSignal) {
      Error_Handler();
    }
    if (osMutexWait(gameStateMutexHandle, osWaitForever) != osOK) {
      Error_Handler();
    }
    snapshot = game_state;
    if (osMutexRelease(gameStateMutexHandle) != osOK) {
      Error_Handler();
    }

    if (draw_grid(&snapshot) == -1) {
      Error_Handler();
    }
  }
  /* USER CODE END StartDrawGridTask */
}

/* USER CODE BEGIN Header_StartPollAcceleroTask */
/**
* @brief Function implementing the pollAcceleroTas thread.
* @param argument: Not used
* @retval None
*/
/* USER CODE END Header_StartPollAcceleroTask */
void StartPollAcceleroTask(void const * argument)
{
  /* USER CODE BEGIN StartPollAcceleroTask */
  uint32_t last = osKernelSysTick();
  int16_t accel[3];
  int32_t neutral_sum[2] = {0};

  // Hold the board still in its neutral position during startup calibration.
  for (int sample = 0; sample < ACCELERO_CALIBRATION_SAMPLES; ++sample) {
    if (osDelayUntil(&last, ACCELERO_POLL_MS) != osOK) {
      Error_Handler();
    }
    BSP_ACCELERO_AccGetXYZ(accel);
    neutral_sum[0] += accel[ACCELERO_LATERAL_AXIS];
    neutral_sum[1] += accel[ACCELERO_DROP_AXIS];
  }
  const int32_t lateral_neutral = neutral_sum[0] / ACCELERO_CALIBRATION_SAMPLES;
  const int32_t drop_neutral = neutral_sum[1] / ACCELERO_CALIBRATION_SAMPLES;
  TiltRepeat lateral_repeat = {0};
  TiltRepeat drop_repeat = {0};

  for(;;)
  {
    if (osDelayUntil(&last, ACCELERO_POLL_MS) != osOK) {
      Error_Handler();
    }
    BSP_ACCELERO_AccGetXYZ(accel);
    const int32_t tilt = ACCELERO_AXIS_SIGN * (accel[ACCELERO_LATERAL_AXIS] - lateral_neutral);
    const int32_t drop_tilt = ACCELERO_DROP_SIGN * (accel[ACCELERO_DROP_AXIS] - drop_neutral);

    uint32_t move = 0;
    if (tilt <= -ACCELERO_DEAD_ZONE_MG) {
      move = INPUT_LEFT;
    } else if (tilt >= ACCELERO_DEAD_ZONE_MG) {
      move = INPUT_RIGHT;
    } else if (lateral_repeat.held_move == INPUT_LEFT && tilt < -ACCELERO_RELEASE_ZONE_MG) {
      move = INPUT_LEFT;
    } else if (lateral_repeat.held_move == INPUT_RIGHT && tilt > ACCELERO_RELEASE_ZONE_MG) {
      move = INPUT_RIGHT;
    }

    uint32_t drop_move = 0;
    if (drop_tilt >= ACCELERO_DEAD_ZONE_MG
        || (drop_repeat.held_move == INPUT_DOWN && drop_tilt > ACCELERO_RELEASE_ZONE_MG)) {
      drop_move = INPUT_DOWN;
    }

    uint32_t now = HAL_GetTick();
    queue_tilt_move(&lateral_repeat, move, now);
    queue_tilt_move(&drop_repeat, drop_move, now);
  }
  /* USER CODE END StartPollAcceleroTask */
}

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM7 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM7)
  {
    HAL_IncTick();
  }
  /* USER CODE BEGIN Callback 1 */

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
#ifdef USE_FULL_ASSERT
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
