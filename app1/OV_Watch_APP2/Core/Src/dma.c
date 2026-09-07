/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    dma.c
  * @brief   This file provides code for the configuration
  *          of all the requested memory to memory DMA transfers.
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
#include "dma.h"

/* USER CODE BEGIN 0 */

/* USER CODE END 0 */

/*----------------------------------------------------------------------------*/
/* Configure DMA                                                              */
/*----------------------------------------------------------------------------*/

/* USER CODE BEGIN 1 */

/*
 * 这个文件由 CubeMX 管理 DMA 控制器的公共基础，不负责描述某个具体数据缓冲区。
 * 当前 APP2 只有 SPI1_TX 使用 DMA2：具体的 hdma_spi1_tx 句柄和 Stream/Channel 参数在 spi.c 的
 * HAL_SPI_MspInit() 中生成；本函数必须先开启 DMA2 时钟和 IRQ，之后 HAL_SPI_Transmit_DMA() 才能工作。
 * main.c 在 MX_SPI1_Init() 之前调用 MX_DMA_Init()，否则 SPI1 虽可初始化但 DMA 请求没有时钟和中断出口。
 */

/* USER CODE END 1 */

/**
  * Enable DMA controller clock
  */
void MX_DMA_Init(void)
{

  /* DMA controller clock enable */
  __HAL_RCC_DMA2_CLK_ENABLE();

  /* DMA interrupt init */
  /* DMA2_Stream2_IRQn interrupt configuration */
  HAL_NVIC_SetPriority(DMA2_Stream2_IRQn, 5, 0);
  HAL_NVIC_EnableIRQ(DMA2_Stream2_IRQn);

}

/* USER CODE BEGIN 2 */

/*
 * 上方 __HAL_RCC_DMA2_CLK_ENABLE() 是 HAL/CMSIS 宏：打开 DMA2 外设时钟；它只让 DMA 寄存器可访问，
 * 不会自己开始内存搬运。SPI1 的 TX DMA 被 CubeMX 放在 DMA2，因此不能错开成 DMA1。
 *
 * HAL_NVIC_SetPriority(DMA2_Stream2_IRQn, 5, 0) 设置 DMA2 Stream2 中断的抢占/子优先级。
 * 抢占优先级 5 与 FreeRTOS 的 configLIBRARY_MAX_SYSCALL_INTERRUPT_PRIORITY=5 相容；本阶段回调
 * 尚未调用 FreeRTOS API，但后续若从 DMA 完成中断通知任务，不需要再改变此优先级边界。
 *
 * HAL_NVIC_EnableIRQ(DMA2_Stream2_IRQn) 允许 CPU 响应这条中断。没有它，DMA 计数可能到零，
 * 但 stm32f4xx_it.c 不会运行 HAL_DMA_IRQHandler()，LCD 的 busy 状态和 CS 将无法被释放。
 */

/* USER CODE END 2 */
