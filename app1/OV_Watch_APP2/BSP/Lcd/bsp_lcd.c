    /* 最小 ST7789 显示 BSP：负责复位与 SPI 命令初始化，不包含 DMA 或 LVGL。 */
    #include "bsp_lcd.h"

    #include "main.h"
    #include "spi.h"

    /* 逻辑显示区域；控制器显存的 Y 偏移将在填屏阶段单独处理。 */
    #define LCD_WIDTH  240U
    #define LCD_HEIGHT 280U

    /* CS 选中 LCD，DC=0 使控制器把 SPI 字节解释为命令。 */
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

    /* RST 低电平复位面板；此时调度器未启动，故使用 HAL_Delay 而非 osDelay。 */
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
