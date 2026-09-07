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

/*
 * 异步启动一帧 RGB565 像素块的 DMA 写入，坐标与 pixels 的布局规则和 Lcd_WritePixels() 相同。
 * true 仅表示参数合法、LCD 空闲且首个 DMA 分块已被 SPI 接受；返回时屏幕可能还在传输，
 * 因此调用者在 Lcd_IsDmaBusy() 返回 false 前不得修改或释放 pixels 指向的数据。
 * 本接口是 LCD DMA 阶段的诊断入口；后续 LVGL 阶段会在此异步完成语义上建立 flush_cb。
 */
bool Lcd_StartWritePixelsDma(uint16_t x, uint16_t y,
                             uint16_t width, uint16_t height,
                             const uint16_t *pixels);

/* 返回 true 表示 SPI1 的 LCD DMA 事务尚未完成，DMA 缓冲区与调用者像素数据仍被驱动占用。 */
bool Lcd_IsDmaBusy(void);

/* 返回最近一次 DMA 事务是否因启动失败或 HAL SPI 错误结束；下一次成功启动会清除此状态。 */
bool Lcd_DmaTransferFailed(void);

/* 全屏填充是 Lcd_FillRect() 的便捷封装。 */
void Lcd_FillScreen(uint16_t color);

#endif
