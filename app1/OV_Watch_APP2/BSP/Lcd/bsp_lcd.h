#ifndef BSP_LCD_H
#define BSP_LCD_H

#include <stdbool.h>
#include <stdint.h>

/* 初始化 ST7789 面板；必须在首次调用任何填色或像素写入接口之前执行一次。 */
void Lcd_Init(void);

/*
 * 用一种 RGB565 颜色填充逻辑坐标中的矩形区域。
 * x：左上角的逻辑列坐标，范围为 0 到 239；y：左上角的逻辑行坐标，范围为 0 到 279。
 * width/height：矩形的像素宽度和高度，均必须大于 0，且整个矩形不能越出显示区域。
 * color：一个 RGB565 颜色值，例如红色 0xF800；非法或越界区域会被忽略。
 */
void Lcd_FillRect(uint16_t x, uint16_t y,
                  uint16_t width, uint16_t height,
                  uint16_t color);

/*
 * x/y/width/height 的坐标和范围规则与 Lcd_FillRect() 相同。
 * pixels：首个像素的地址；pixels[row * width + column] 对应窗口中的 (x + column, y + row)。
 * true 表示参数合法且阻塞传输已完成；false 表示空指针或矩形越界。
 */
bool Lcd_WritePixels(uint16_t x, uint16_t y,
                     uint16_t width, uint16_t height,
                     const uint16_t *pixels);

/* 全屏填充是 Lcd_FillRect() 的便捷封装。 */
void Lcd_FillScreen(uint16_t color);

#endif
