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

/*
 * 默认 UI 任务可使用的栈容量，单位为 32-bit word；1024 words 在 STM32F411 上等于 4096 B。
 * StartDefaultTask() 要执行 LVGL 的对象创建、布局、绘制、SPI 刷新和 FreeRTOS 延时，原来的
 * 128 words/512 B 已实测会向下覆盖相邻的 FreeRTOS 定时器任务控制块并导致 PendSV HardFault。
 * 该常量由 defaultTask_attributes 读取，CubeMX 的 .ioc 中也保存相同的 1024-word 配置；后续应
 * 依据 default_task_stack_min_free_words 的实测最小余量调整，而不是凭感觉继续增减。
 */
#define APP2_DEFAULT_TASK_STACK_WORDS  (1024U)

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
/* USER CODE BEGIN Variables */
volatile bool key1_pressed;
volatile bool wake_key_pressed;
/*
 * default_task_stack_min_free_words 保存 defaultTask 自启动以来剩余栈空间的历史最小值，单位为
 * FreeRTOS StackType_t word（本 MCU 为 4 B）。它只由 StartDefaultTask() 写入，调试器读取；
 * 值越小表示越接近栈底，0 表示已经没有可证明的余量。它是运行时诊断量，不参与页面业务逻辑。
 */
volatile UBaseType_t default_task_stack_min_free_words;
/*
 * rtos_stack_overflow_detected 是 FreeRTOS 检测到任务栈越界后的冻结标记。栈溢出钩子写入 true，
 * 调试器读取；正常运行时保持 false。它不尝试恢复已经不可信的任务上下文，只留下可定位证据。
 */
volatile bool rtos_stack_overflow_detected;
/*
 * rtos_stack_overflow_task_name 指向触发栈溢出检测的任务名字符串。钩子写入、调试器读取；字符串
 * 由 FreeRTOS TCB 持有，任务存在期间有效。它只用于故障定位，不能作为跨任务通信数据。
 */
volatile const char *rtos_stack_overflow_task_name;

/* USER CODE END Variables */
/* Definitions for defaultTask */
osThreadId_t defaultTaskHandle;
const osThreadAttr_t defaultTask_attributes = {
  .name = "defaultTask",
  /* CMSIS-RTOS2 的 stack_size 单位为字节，因此把上面的 word 数乘以 sizeof(uint32_t)。 */
  .stack_size = APP2_DEFAULT_TASK_STACK_WORDS * sizeof(uint32_t),
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

  /*
   * uxTaskGetStackHighWaterMark(NULL) 查询当前 defaultTask 历史上最少剩余的栈 word 数；NULL 表示
   * 当前调用任务。它只读取 FreeRTOS 的栈填充标记，不会分配内存也不会修复溢出，必须在任务上下文
   * 调用。初始化后立即记录一次，建立 UI 创建阶段的栈余量基线。
   */
  default_task_stack_min_free_words = uxTaskGetStackHighWaterMark(NULL);

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
    /*
     * 再次读取历史最小余量，使调试器能看到包括本轮 LVGL 绘制和 flush 调用在内的最深栈使用量。
     * 这个 API 的返回值是“历史最低剩余量”而非瞬时可用量，所以数值只会保持或减小，便于稳定判断。
     */
    default_task_stack_min_free_words = uxTaskGetStackHighWaterMark(NULL);
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

/*
 * FreeRTOS 在任务切换边界发现栈填充区被破坏时调用此钩子。
 * xTask 是发生越界的任务句柄，pcTaskName 是该任务控制块内的名称；本工程只记录名称和故障标记后
 * 关闭中断并停止，原因是继续调度可能会使用已损坏的 TCB 或返回地址，掩盖最初错误。该钩子不是
 * 异常恢复机制；它的作用是把“随机 HardFault”收敛为可重复观察的任务栈问题。
 */
void vApplicationStackOverflowHook(TaskHandle_t xTask, char *pcTaskName)
{
  /* 当前诊断不需要再解引用任务句柄；保留参数是 FreeRTOS 规定的回调签名。 */
  (void)xTask;
  /* 写入顺序使调试器先能确认故障，再读取对应任务名。 */
  rtos_stack_overflow_detected = true;
  rtos_stack_overflow_task_name = pcTaskName;
  /* taskDISABLE_INTERRUPTS() 禁止可屏蔽中断，防止已损坏的调度状态继续产生任务切换。 */
  taskDISABLE_INTERRUPTS();
  for (;;)
  {
    /* 故障后留在此处供 ARM GDB/CLion 读取诊断变量；不返回 FreeRTOS。 */
  }
}

/* Private application code --------------------------------------------------*/
/* USER CODE BEGIN Application */

/* USER CODE END Application */

