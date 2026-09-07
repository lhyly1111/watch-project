#ifndef BSP_LCD_H
#define BSP_LCD_H

#include <stdbool.h>
#include <stdint.h>

void Lcd_Init(void);

/*
 * 用一种 RGB565 颜色填充逻辑坐标中的矩形区域。
 * x、y 是左上角；width、height 是区域尺寸。非法或越界区域会被忽略。
 */
void Lcd_FillRect(uint16_t x, uint16_t y,
                  uint16_t width, uint16_t height,
                  uint16_t color);

/*
 * 将按“从左到右、从上到下”排列的 RGB565 像素块写入指定矩形。
 * true 表示参数合法且阻塞传输已完成；false 表示空指针或矩形越界。
 */
bool Lcd_WritePixels(uint16_t x, uint16_t y,
                     uint16_t width, uint16_t height,
                     const uint16_t *pixels);

/* 全屏填充是 Lcd_FillRect() 的便捷封装。 */
void Lcd_FillScreen(uint16_t color);

#endif
