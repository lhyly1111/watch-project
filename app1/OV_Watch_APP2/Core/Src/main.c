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
#include "adc.h"
#include "spi.h"
#include "tim.h"
#include "gpio.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "bsp_backlight.h"
#include "bsp_battery.h"
#include "bsp_lcd.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */
/* 测试图案的横向像素数量；它决定每一行在 lcd_pixel_test_buffer 中包含多少个颜色值。 */
#define LCD_PIXEL_TEST_WIDTH   96U
/* 测试图案的纵向像素数量；与宽度相乘得到总像素数和测试缓冲区容量。 */
#define LCD_PIXEL_TEST_HEIGHT  60U
/* 测试图案左上角的逻辑列坐标；24 让图案离屏幕左边缘留出黑色背景便于定位。 */
#define LCD_PIXEL_TEST_X       24U
/* 测试图案左上角的逻辑行坐标；40 让图案离屏幕上边缘留出黑色背景便于定位。 */
#define LCD_PIXEL_TEST_Y       40U

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/

/* USER CODE BEGIN PV */
/* battery_raw 保存 ADC1 最近一次原始转换值，单位是 ADC 计数；volatile 防止编译器缓存被硬件更新的数据。 */
volatile uint16_t battery_raw;
/* battery_voltage 保存由原始 ADC 值换算出的电池电压，单位为 V，供后续电池状态页面使用。 */
volatile float battery_voltage;
/*
 * 本次测试图案的像素存储区：96 x 60 个 uint16_t，每个元素存一个 RGB565 颜色，
 * 共占 96 x 60 x 2 = 11520 字节（约 11.25 KB）。
 * static 使数组位于全局数据区而非 main() 的栈；图案函数填充它后，
 * Lcd_WritePixels() 才能在阻塞发送期间持续读取其中的像素数据。
 */
static uint16_t lcd_pixel_test_buffer[LCD_PIXEL_TEST_WIDTH * LCD_PIXEL_TEST_HEIGHT];
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
void MX_FREERTOS_Init(void);
/* USER CODE BEGIN PFP */

/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/*
 * 构造不对称图案：红色顶条、绿色左条、蓝色右中块、白色下中块。
 * 不对称形状能同时暴露行列颠倒、上下翻转和左右翻转等问题。
 */
static void Lcd_BuildPixelTestPattern(void)
{
  /* y 是正在生成的图案行号，范围为 0 到 59；外层循环每执行一次就完成一整行。 */
  for (uint16_t y = 0U; y < LCD_PIXEL_TEST_HEIGHT; ++y)
  {
    /* x 是当前行内的列号，范围为 0 到 95；内层循环决定该位置最终写入哪种颜色。 */
    for (uint16_t x = 0U; x < LCD_PIXEL_TEST_WIDTH; ++x)
    {
      /* color 是当前 (x, y) 像素的 RGB565 颜色；先设为黑色，再由条件覆盖为测试色块。 */
      uint16_t color = 0x0000U;

      /* y < 10 表示图案最上方 10 行，用红色建立容易识别的顶部方向标记。 */
      if (y < 10U)
      {
        color = 0xF800U;
      }
      /* x < 20 表示红条以下的最左侧 20 列，用绿色建立左侧方向标记。 */
      else if (x < 20U)
      {
        color = 0x07E0U;
      }
      /* x/y 范围限定右侧中部蓝块；它不与左条或顶条对称，便于发现翻转。 */
      else if ((x >= 64U) && (y >= 20U) && (y < 40U))
      {
        color = 0x001FU;
      }
      /* x/y 范围限定下方中部白块；与蓝块一起提供第二个方向参考。 */
      else if ((x >= 32U) && (x < 48U) && (y >= 40U))
      {
        color = 0xFFFFU;
      }

      /* C 数组按行优先存放：先写完一行，再进入下一行。 */
      lcd_pixel_test_buffer[y * LCD_PIXEL_TEST_WIDTH + x] = color;
    }
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
  MX_TIM3_Init();
  MX_ADC1_Init();
  MX_SPI1_Init();
  /* USER CODE BEGIN 2 */
  Backlight_Init();
  battery_raw = Battery_ReadRaw();
  battery_voltage = Battery_ReadVoltageAverage();
  Lcd_Init();
  /*
   * LCD 驱动 V1 的第二项实物测试：先清黑屏，再写入一块不对称像素图案。
   * 这会验证 uint16_t 像素数组的字节序、行优先顺序和显示位置。
   */
  Lcd_FillScreen(0x0000U);//全屏填充黑色
  Lcd_BuildPixelTestPattern();
  if (!Lcd_WritePixels(LCD_PIXEL_TEST_X, LCD_PIXEL_TEST_Y,
                       LCD_PIXEL_TEST_WIDTH, LCD_PIXEL_TEST_HEIGHT,
                       lcd_pixel_test_buffer))
  {
    Error_Handler();
  }
  /* USER CODE END 2 */

  /* Init scheduler */
  osKernelInitialize();  /* Call init function for freertos objects (in cmsis_os2.c) */
  MX_FREERTOS_Init();

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
  __HAL_RCC_PWR_CLK_ENABLE();
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSI;
  RCC_OscInitStruct.HSIState = RCC_HSI_ON;
  RCC_OscInitStruct.HSICalibrationValue = RCC_HSICALIBRATION_DEFAULT;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSI;
  RCC_OscInitStruct.PLL.PLLM = 8;
  RCC_OscInitStruct.PLL.PLLN = 100;
  RCC_OscInitStruct.PLL.PLLP = RCC_PLLP_DIV2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
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
  RCC_ClkInitStruct.APB1CLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_HCLK_DIV1;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_3) != HAL_OK)
  {
    Error_Handler();
  }
}

/* USER CODE BEGIN 4 */

/* USER CODE END 4 */

/**
  * @brief  Period elapsed callback in non blocking mode
  * @note   This function is called  when TIM1 interrupt took place, inside
  * HAL_TIM_IRQHandler(). It makes a direct call to HAL_IncTick() to increment
  * a global variable "uwTick" used as application time base.
  * @param  htim : TIM handle
  * @retval None
  */
void HAL_TIM_PeriodElapsedCallback(TIM_HandleTypeDef *htim)
{
  /* USER CODE BEGIN Callback 0 */

  /* USER CODE END Callback 0 */
  if (htim->Instance == TIM1)
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
