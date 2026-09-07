#ifndef LV_PORT_DISP_H
#define LV_PORT_DISP_H

/*
 * LVGL 显示端口只负责把 LVGL 的刷新矩形交给 APP2 LCD BSP。
 * 它不初始化 SPI/LCD，不创建页面，也不决定 UI 刷新频率；这些职责分别留给 main.c、BSP/Lcd 和 UI 任务。
 */

/*
 * 注册 APP2 的唯一 LVGL 显示设备和绘制缓冲区。
 * 调用顺序必须是 lv_init() 之后、创建任何 LVGL 控件之前；LCD 也必须已由 main.c 的 Lcd_Init() 初始化。
 */
void LvPortDisp_Init(void);

#endif
