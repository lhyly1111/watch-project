#ifndef APP_LVGL_DEMO_H
#define APP_LVGL_DEMO_H

/*
 * 创建 APP2 当前阶段的最小 LVGL 证据页面。
 * 调用者必须先完成 lv_init() 和 LvPortDisp_Init()；本页面不直接操作 SPI、DMA 或 LCD 控制脚。
 */
void AppLvglDemo_Create(void);

#endif
