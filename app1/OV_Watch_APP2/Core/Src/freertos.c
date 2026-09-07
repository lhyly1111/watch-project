/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * File Name          : freertos.c
  * Description        : Code for freertos applications
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
#include "FreeRTOS.h"
#include "task.h"
#include "main.h"
#include "cmsis_os.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include "bsp_key.h"
#include "app_lvgl_demo.h"
#include "lv_port_disp.h"
#include "lvgl.h"

/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
volatile bool key1_pressed;
volatile bool wake_key_pressed;

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  .stack_size = 128 * 4,
  .priority = (osPriority_t) osPriorityNormal,
};

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN FunctionPrototypes */

/* USER CODE END FunctionPrototypes */

void StartDefaultTask(void *argument);

void MX_FREERTOS_Init(void); /* (MISRA C 2004 rule 8.1) */

/**
  * @brief  FreeRTOS initialization
  * @param  None
  * @retval None
  */
void MX_FREERTOS_Init(void) {
  /* USER CODE BEGIN Init */

  /* USER CODE END Init */

  /* USER CODE BEGIN RTOS_MUTEX */
  /* add mutexes, ... */
  /* USER CODE END RTOS_MUTEX */

  /* USER CODE BEGIN RTOS_SEMAPHORES */
  /* add semaphores, ... */
  /* USER CODE END RTOS_SEMAPHORES */

  /* USER CODE BEGIN RTOS_TIMERS */
  /* start timers, add new ones, ... */
  /* USER CODE END RTOS_TIMERS */

  /* USER CODE BEGIN RTOS_QUEUES */
  /* add queues, ... */
  /* USER CODE END RTOS_QUEUES */

  /* Create the thread(s) */
  /* creation of defaultTask */
  defaultTaskHandle = osThreadNew(StartDefaultTask, NULL, &defaultTask_attributes);

  /* USER CODE BEGIN RTOS_THREADS */
  /* add threads, ... */
  /* USER CODE END RTOS_THREADS */

  /* USER CODE BEGIN RTOS_EVENTS */
  /* add events, ... */
  /* USER CODE END RTOS_EVENTS */

}

/* USER CODE BEGIN Header_StartDefaultTask */
/**
  * @brief  Function implementing the defaultTask thread.
  * @param  argument: Not used
  * @retval None
  */
/* USER CODE END Header_StartDefaultTask */
void StartDefaultTask(void *argument)
{
  /* USER CODE BEGIN StartDefaultTask */
  /*
   * lv_init() 初始化 LVGL 的对象、样式、内存和内部定时器系统；必须只执行一次，且先于所有 lv_obj_* 调用。
   * 它不初始化 SPI 或 LCD，main.c 已在启动调度器前完成 Lcd_Init()；调用成功只表示图形库状态可用，不表示画面已显示。
   */
  lv_init();
  /* LvPortDisp_Init() 注册 240 x 280 的单缓冲显示设备和阻塞 flush_cb；它必须晚于 lv_init()。 */
  LvPortDisp_Init();
  /* AppLvglDemo_Create() 创建当前唯一的测试页面和 1 秒 LVGL 软件定时器；此后不能由其他任务直接修改这些对象。 */
  AppLvglDemo_Create();

  /* Infinite loop */
  for(;;)
  {
    /* key1_pressed 保存 KEY1 当前轮询状态；按键尚未接入 LVGL 输入设备，本阶段仅保留既有读取行为。 */
    key1_pressed = Key1_IsPressed();
    /* wake_key_pressed 保存 WAKE 当前轮询状态；它同样尚未参与 LVGL 事件分发。 */
    wake_key_pressed = WakeKey_IsPressed();

    /*
     * lv_timer_handler() 在唯一 UI 任务中处理 LVGL 软件定时器、dirty 区域绘制和 flush_cb 调用，并返回建议的下次处理间隔。
     * LVGL 的时间来自 lv_conf.h 中的 HAL_GetTick()，本调用不应放入中断；返回 0 只表示尽快再运行，仍要至少 osDelay(1) 让出 CPU。
     */
    uint32_t lvgl_next_delay_ms = lv_timer_handler();
    /* 将较长建议延迟限制为 10 ms，既避免空转，也让按键轮询和页面刷新保持较低响应延迟。 */
    if (lvgl_next_delay_ms > 10U)
    {
      lvgl_next_delay_ms = 10U;
    }
    /* CMSIS-RTOS2 的 osDelay() 以毫秒为单位阻塞当前任务而非整个 MCU；0 会变成 1，保证不忙等。 */
    osDelay((lvgl_next_delay_ms == 0U) ? 1U : lvgl_next_delay_ms);
  }
  /* USER CODE END StartDefaultTask */
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

