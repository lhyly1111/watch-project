    /* 最小 ST7789 显示 BSP：负责复位与 SPI 命令初始化，不包含 DMA 或 LVGL。 */
    #include "bsp_lcd.h"

    #include <stdbool.h>

    #include "main.h"
    #include "spi.h"

    /* 逻辑显示区域；控制器显存的 Y 偏移将在填屏阶段单独处理。 */
    #define LCD_WIDTH  240U
    #define LCD_HEIGHT 280U
    #define LCD_X_OFFSET 0U
    #define LCD_Y_OFFSET 20U
    #define LCD_FILL_CHUNK_PIXELS 256U

    /* CS 选中 LCD，DC=0 使控制器把 SPI 字节解释为命令。
       命令：CS=0 -> DC=0 -> SPI 发命令字节 -> CS=1
       数据：CS=0 -> DC=1 -> SPI 发参数/像素字节 -> CS=1
     */
    static void Lcd_WriteCommand(uint8_t command)
    {
        HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_RESET);

        if (HAL_SPI_Transmit(&hspi1, &command, 1U, 100U) != HAL_OK) {
            Error_Handler();
        }

        HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
    }

    /* DC=1 表示命令参数或像素数据；SPI 传输失败后停在 Error_Handler。 */
    static void Lcd_WriteData(const uint8_t *data, uint16_t size)
    {
        HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);
        HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET);

        if (HAL_SPI_Transmit(&hspi1, (uint8_t *)data, size, 100U) != HAL_OK) {
            Error_Handler();
        }

        HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
    }

    /* RST 低电平复位面板；此时调度器未启动，故使用 HAL_Delay 而非 osDelay。
       RST=0，保持 100 ms：让面板进入硬件复位状态
       RST=1，等待 120 ms：让面板完成内部启动
    */
    static void Lcd_Reset(void)
    {
        HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_RESET);
        HAL_Delay(100U);
        HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_SET);
        HAL_Delay(120U);
    }
    /* ST7789 的常见帧格式：一条命令，后跟零个或多个参数字节。 */
    static void Lcd_WriteCommandWithData(uint8_t command,
                                         const uint8_t *data,
                                         uint16_t size)
        {
            Lcd_WriteCommand(command);

            if (size > 0U) {
                Lcd_WriteData(data, size);
            }
        }

    static void Lcd_SetAddressWindow(uint16_t x_start,
                                     uint16_t y_start,
                                     uint16_t x_end,
                                     uint16_t y_end)
        {
            const uint16_t controller_x_start = x_start + LCD_X_OFFSET;
            const uint16_t controller_x_end = x_end + LCD_X_OFFSET;
            const uint16_t controller_y_start = y_start + LCD_Y_OFFSET;
            const uint16_t controller_y_end = y_end + LCD_Y_OFFSET;

            const uint8_t column_data[] = {
                (uint8_t)(controller_x_start >> 8U),
                (uint8_t)controller_x_start,
                (uint8_t)(controller_x_end >> 8U),
                (uint8_t)controller_x_end
            };
            const uint8_t row_data[] = {
                (uint8_t)(controller_y_start >> 8U),
                (uint8_t)controller_y_start,
                (uint8_t)(controller_y_end >> 8U),
                (uint8_t)controller_y_end
            };

            Lcd_WriteCommandWithData(0x2AU, column_data, sizeof(column_data));
            Lcd_WriteCommandWithData(0x2BU, row_data, sizeof(row_data));
            Lcd_WriteCommand(0x2CU);
        }

    /*
     * 检查逻辑坐标是否完整落在 240 x 280 的可见区域中。
     * 先检查 x/y，再用 "宽度 <= 屏幕宽度 - x" 判断，避免 x + width
     * 在 uint16_t 中溢出后错误地通过判断。
     */
    static bool Lcd_IsRectValid(uint16_t x, uint16_t y,
                                 uint16_t width, uint16_t height)
    {
        return (width > 0U) && (height > 0U) &&
               (x < LCD_WIDTH) && (y < LCD_HEIGHT) &&
               (width <= (LCD_WIDTH - x)) &&
               (height <= (LCD_HEIGHT - y));
    }

    void Lcd_FillRect(uint16_t x, uint16_t y,
                      uint16_t width, uint16_t height,
                      uint16_t color)
    {
        /*
         * 使用静态小缓冲区而不是按整个矩形分配：整屏需要 134400 字节，
         * 超出栈空间；这里反复发送同一块 256 像素的颜色数据即可。
         */
        static uint8_t pixel_buffer[LCD_FILL_CHUNK_PIXELS * 2U];
        uint32_t remaining_pixels;
        uint16_t pixel_count;

        if (!Lcd_IsRectValid(x, y, width, height)) {
            return;
        }

        /* ST7789 要求 RGB565 高字节先通过 SPI 发出。 */
        for (uint16_t index = 0U; index < LCD_FILL_CHUNK_PIXELS; ++index) {
            pixel_buffer[index * 2U] = (uint8_t)(color >> 8U);
            pixel_buffer[index * 2U + 1U] = (uint8_t)color;
        }

        /* width/height 是尺寸，x + width - 1/y + height - 1 才是右下角坐标。 */
        Lcd_SetAddressWindow(x, y, x + width - 1U, y + height - 1U);
        remaining_pixels = (uint32_t)width * height;

        while (remaining_pixels > 0U) {
            if (remaining_pixels > LCD_FILL_CHUNK_PIXELS) {
                pixel_count = LCD_FILL_CHUNK_PIXELS;
            } else {
                pixel_count = (uint16_t)remaining_pixels;
            }

            Lcd_WriteData(pixel_buffer, pixel_count * 2U);
            remaining_pixels -= pixel_count;
        }
    }

    void Lcd_FillScreen(uint16_t color)
    {
        /* 统一走矩形接口，保证全屏与局部区域遵循同一套窗口逻辑。 */
        Lcd_FillRect(0U, 0U, LCD_WIDTH, LCD_HEIGHT, color);
    }
    /*
       * 参考 V2.4.3 的 ST7789 初始化表。
       * 在首屏验证前保持这些参数不变，避免同时引入“面板参数”和“连线”两类变量。
       */
    void Lcd_Init(void)
        {
            /* 0x36 控制扫描方向；0x00 是参考工程的竖屏配置。 */
            static const uint8_t madctl[] = {0x00U};
            /* 0x3A 的 0x05 选择 RGB565，每个像素占 16 bit。 */
            static const uint8_t colmod[] = {0x05U};

            /* B2-D0 是显示时序、电源与电压参数，先保持参考值。 */
            static const uint8_t b2[] = {0x0CU, 0x0CU, 0x00U, 0x33U, 0x33U};
            static const uint8_t b7[] = {0x35U};
            static const uint8_t bb[] = {0x19U};
            static const uint8_t c0[] = {0x2CU};
            static const uint8_t c2[] = {0x01U};
            static const uint8_t c3[] = {0x12U};
            static const uint8_t c4[] = {0x20U};
            static const uint8_t c6[] = {0x0FU};
            static const uint8_t d0[] = {0xA4U, 0xA1U};
            /* E0/E1 是正、负 Gamma 曲线，不是将要写入屏幕的颜色数据。 */
            static const uint8_t e0[] = {
                0xD0U, 0x04U, 0x0DU, 0x11U, 0x13U, 0x2BU, 0x3FU,
                0x54U, 0x4CU, 0x18U, 0x0DU, 0x0BU, 0x1FU, 0x23U
            };
            static const uint8_t e1[] = {
                0xD0U, 0x04U, 0x0CU, 0x11U, 0x13U, 0x2CU, 0x3FU,
                0x44U, 0x51U, 0x2FU, 0x1FU, 0x1FU, 0x20U, 0x23U
            };

            Lcd_Reset();

            /* 0x11 退出睡眠后，控制器要求至少等待 120 ms。 */
            Lcd_WriteCommand(0x11U);
            HAL_Delay(120U);

            Lcd_WriteCommandWithData(0x36U, madctl, sizeof(madctl));
            Lcd_WriteCommandWithData(0x3AU, colmod, sizeof(colmod));
            Lcd_WriteCommandWithData(0xB2U, b2, sizeof(b2));
            Lcd_WriteCommandWithData(0xB7U, b7, sizeof(b7));
            Lcd_WriteCommandWithData(0xBBU, bb, sizeof(bb));
            Lcd_WriteCommandWithData(0xC0U, c0, sizeof(c0));
            Lcd_WriteCommandWithData(0xC2U, c2, sizeof(c2));
            Lcd_WriteCommandWithData(0xC3U, c3, sizeof(c3));
            Lcd_WriteCommandWithData(0xC4U, c4, sizeof(c4));
            Lcd_WriteCommandWithData(0xC6U, c6, sizeof(c6));
            Lcd_WriteCommandWithData(0xD0U, d0, sizeof(d0));
            Lcd_WriteCommandWithData(0xE0U, e0, sizeof(e0));
            Lcd_WriteCommandWithData(0xE1U, e1, sizeof(e1));

            /* 0x21 开启反色；0x29 打开显示输出。 */
            Lcd_WriteCommand(0x21U);
            Lcd_WriteCommand(0x29U);
        }
