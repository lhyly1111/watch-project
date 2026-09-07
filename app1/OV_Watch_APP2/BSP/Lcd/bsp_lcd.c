    /* 最小 ST7789 显示 BSP：负责复位、SPI 命令、阻塞像素写入和 DMA 像素写入，不包含 LVGL。 */
    #include "bsp_lcd.h"

    #include <stdbool.h>
    #include <stddef.h>

    #include "main.h"
    #include "spi.h"

    /* APP2 对上层公开的可见逻辑宽度，单位为像素；x 坐标合法范围是 0 到 239。 */
    #define LCD_WIDTH  240U
    /* APP2 对上层公开的可见逻辑高度，单位为像素；y 坐标合法范围是 0 到 279。 */
    #define LCD_HEIGHT 280U
    /* 逻辑 x 坐标到 ST7789 显存列坐标的偏移；当前面板列坐标无需补偿，故为 0。 */
    #define LCD_X_OFFSET 0U
    /* 逻辑 y 坐标到 ST7789 显存行坐标的偏移；当前 240 x 280 可视区从控制器第 20 行开始。 */
    #define LCD_Y_OFFSET 20U
    /* 每次 SPI 阻塞发送最多复用的像素数量；256 像素等于 512 字节，避免大栈缓冲区和单次超长传输。 */
    #define LCD_FILL_CHUNK_PIXELS 256U

    /*
     * lcd_dma_transfer_buffer 保存一个待 DMA 发送的高字节优先 RGB565 分块，容量为 256 像素/512 字节。
     * 它必须是 static：HAL_SPI_Transmit_DMA() 返回后 DMA 仍会读取这个 RAM 区域，不能使用已离开作用域的栈数组。
     * 仅 Lcd_StartNextDmaChunk() 写入，DMA 繁忙期间只能被 DMA 外设读取；一帧结束后才能被下一帧覆盖。
     */
    static uint8_t lcd_dma_transfer_buffer[LCD_FILL_CHUNK_PIXELS * 2U];
    /*
     * lcd_dma_busy 是 LCD DMA 事务的所有权标记。主程序通过 Lcd_IsDmaBusy() 读取它，
     * DMA 完成/错误中断通过回调清除它；volatile 防止编译器把被中断异步修改的值缓存到寄存器。
     * false 表示驱动可以接收一帧新像素；true 表示 CS 已被 DMA 事务占用，禁止启动第二帧。
     */
    static volatile bool lcd_dma_busy;
    /*
     * lcd_dma_failed 记录最近一次 DMA 事务是否异常结束。启动新事务时清零，错误回调或启动失败时置位；
     * 主程序可在 lcd_dma_busy 变为 false 后读取它判断本帧是否成功。它同样由中断修改，故使用 volatile。
     */
    static volatile bool lcd_dma_failed;
    /*
     * lcd_dma_current_pixel 指向本帧尚未转换的第一个 uint16_t RGB565 像素。
     * 启动函数从调用者 pixels 赋值；每发送一个分块前推进；DMA 完成或失败后清为空指针，表示本帧不再持有源数据。
     */
    static const uint16_t *lcd_dma_current_pixel;
    /*
     * lcd_dma_remaining_pixels 是当前帧还未复制进 DMA 字节缓冲区的像素数量，而不是字节数量。
     * 它由启动函数设置为 width * height，并由每个 DMA 分块递减；uint32_t 覆盖整屏 67200 像素而不会溢出。
     */
    static uint32_t lcd_dma_remaining_pixels;

    /*
       command：要执行的单字节 ST7789 命令，例如 0x2A（列地址）或 0x29（开显示）。
       CS 选中 LCD，DC=0 使控制器把 SPI 字节解释为命令。
       命令：CS=0 -> DC=0 -> SPI 发命令字节 -> CS=1
       数据：CS=0 -> DC=1 -> SPI 发参数/像素字节 -> CS=1
     */
    static void Lcd_WriteCommand(uint8_t command)
    {
        /* HAL_GPIO_WritePin(端口, 引脚, 电平)：将 CS 拉低，选中 LCD，后续 SPI 字节才会被面板接收。 */
        HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);
        /* 将 DC 拉低，告诉 ST7789 下一个 SPI 字节是“命令”而非参数或像素数据。 */
        HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_RESET);

        /*
         * HAL_SPI_Transmit(SPI 句柄, 发送缓冲区, 字节数, 超时毫秒数) 是 HAL 的阻塞式发送函数。
         * &hspi1 指向 CubeMX 初始化的 SPI1；&command 指向本次唯一的命令字节；1U 表示只发 1 字节；
         * 100U 是最长等待 100 ms。函数返回 HAL_OK 才说明 MCU 侧 SPI 发送完成，不代表面板一定显示正确。
         */
        if (HAL_SPI_Transmit(&hspi1, &command, 1U, 100U) != HAL_OK) {
            /* Error_Handler() 是 CubeMX 项目的故障终止入口；当前实现会关中断并停在死循环，防止继续发送失序数据。 */
            Error_Handler();
        }

        /* 将 CS 拉高，取消本次命令事务；下一次命令或数据会重新选中 LCD。 */
        HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
    }

    /*
     * data：待发送字节流的起始地址，可以指向命令参数或 RGB565 像素数据。
     * size：从 data 开始要发送的字节数，不是 RGB565 像素数；一个像素通常对应两个字节。
     * DC=1 表示命令参数或像素数据；SPI 传输失败后停在 Error_Handler，避免程序带着不完整画面继续运行。
     */
    static void Lcd_WriteData(const uint8_t *data, uint16_t size)
    {
        /* 选中 LCD，保证 data 指向的字节会送到面板而不是被 SPI 总线忽略。 */
        HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);
        /* DC 拉高，告诉 ST7789 后续字节是当前命令的参数，或 0x2C 后的 RGB565 像素数据。 */
        HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET);

        /*
         * 将 data 指向的 size 个字节经 hspi1 阻塞发出，最长等待 100 ms。
         * 这里 size 是字节数：例如 256 个 RGB565 像素需要传入 512，而不是 256。
         * 发送完成后才能复用调用者的缓冲区，这也是当前阻塞实现比 DMA 简单的原因。
         */
        if (HAL_SPI_Transmit(&hspi1, (uint8_t *)data, size, 100U) != HAL_OK) {
            /* SPI 外设超时或错误后停止程序；否则上层会错误地认为整块像素已写入。 */
            Error_Handler();
        }

        /* 数据事务结束后释放 CS；地址窗口保持在 ST7789 内部，下一块数据仍会写入同一窗口的后续位置。 */
        HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
    }

    /* RST 低电平复位面板；此时调度器未启动，故使用 HAL_Delay 而非 osDelay。
       RST=0，保持 100 ms：让面板进入硬件复位状态
       RST=1，等待 120 ms：让面板完成内部启动
    */
    static void Lcd_Reset(void)
    {
        /* 将 RST 拉低，硬件复位 ST7789，清除之前上电或上次运行留下的内部状态。 */
        HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_RESET);
        /* HAL_Delay(100U) 以 HAL 系统 tick 为单位阻塞 100 ms，满足面板复位低电平保持时间。 */
        HAL_Delay(100U);
        /* 将 RST 拉高，允许控制器从复位状态重新启动。 */
        HAL_GPIO_WritePin(LCD_RST_GPIO_Port, LCD_RST_Pin, GPIO_PIN_SET);
        /* 再等待 120 ms，给面板内部电源和状态机完成启动；此时不能提前发送初始化命令。 */
        HAL_Delay(120U);
    }
    /*
     * command：本帧先发送的 ST7789 命令字节。
     * data：紧随该命令的参数首地址；当 size 为 0 时不会读取它。
     * size：参数字节数。例如 0x2A 和 0x2B 的参数长度都是 4，0x11 没有参数。
     * 该函数把“先命令、后数据”的固定协议顺序封装起来，调用者不用重复设置 DC。
     */
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
     * 将逻辑矩形的左上/右下坐标转换为 ST7789 的列地址、行地址后启动 RAM 写入。
     * x_start/y_start 是包含在窗口内的起点；x_end/y_end 是同样包含在窗口内的终点，
     * 因此调用者必须先保证终点没有越界。0x2A 设置列、0x2B 设置行、0x2C 让后续数据变为像素流。
     */
    static void Lcd_SetAddressWindow(uint16_t x_start,
                                     uint16_t y_start,
                                     uint16_t x_end,
                                     uint16_t y_end)
        {
            /* controller_* 保存加过面板显存偏移后的实际 ST7789 坐标，不再是 APP2 的逻辑坐标。 */
            const uint16_t controller_x_start = x_start + LCD_X_OFFSET;
            const uint16_t controller_x_end = x_end + LCD_X_OFFSET;
            const uint16_t controller_y_start = y_start + LCD_Y_OFFSET;
            const uint16_t controller_y_end = y_end + LCD_Y_OFFSET;

            /* column_data 是 0x2A 的四个参数：起始列高/低字节、结束列高/低字节。 */
            const uint8_t column_data[] = {
                (uint8_t)(controller_x_start >> 8U),
                (uint8_t)controller_x_start,
                (uint8_t)(controller_x_end >> 8U),
                (uint8_t)controller_x_end
            };
            /* row_data 是 0x2B 的四个参数：起始行高/低字节、结束行高/低字节。 */
            const uint8_t row_data[] = {
                (uint8_t)(controller_y_start >> 8U),
                (uint8_t)controller_y_start,
                (uint8_t)(controller_y_end >> 8U),
                (uint8_t)controller_y_end
            };

            /* 0x2A 接收 column_data，确定本次像素流可自动递增的列范围。 */
            Lcd_WriteCommandWithData(0x2AU, column_data, sizeof(column_data));
            /* 0x2B 接收 row_data，确定本次像素流可自动递增的行范围。 */
            Lcd_WriteCommandWithData(0x2BU, row_data, sizeof(row_data));
            /* 0x2C 不带参数；从这一刻开始 Lcd_WriteData() 的字节会写入刚设置的显存窗口。 */
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

    /*
     * failed 表示本帧结束原因：false 是全部像素已由 HAL 完成发送，true 是 DMA 未能启动或 SPI 报错。
     * 该函数只能在 DMA 完成/错误回调或同步启动失败路径调用；它统一归还 CS、源缓冲区和 busy 所有权，
     * 防止不同退出路径遗留 CS 低电平或永远 busy 的状态。
     */
    static void Lcd_FinishDmaTransfer(bool failed)
    {
        /* DMA 事务结束后拉高 CS，结束从 0x2C 开始的整段像素数据事务。 */
        HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_SET);
        /* 先保存本帧最终结果，供主程序在 Lcd_IsDmaBusy() 为 false 后查询。 */
        lcd_dma_failed = failed;
        /* 像素源已不再被 DMA 或驱动读取，清空指针可避免调试时误认为它仍被持有。 */
        lcd_dma_current_pixel = NULL;
        /* 剩余数量归零，保证下一帧不会继续上一帧的进度。 */
        lcd_dma_remaining_pixels = 0U;
        /* 最后清 busy，向主程序宣布 LCD 和 DMA 缓冲区已被释放。 */
        lcd_dma_busy = false;
    }

    /*
     * 准备并启动本帧的下一个 DMA 分块。调用者必须已经设置好地址窗口、CS=0、DC=1 且 lcd_dma_busy=true。
     * 返回 true 表示 HAL 已接收 DMA 请求；返回 false 表示启动失败，调用者必须调用 Lcd_FinishDmaTransfer(true)。
     * 此函数既由主程序启动首块，也由 SPI DMA 完成回调启动后续块，因此其中不能阻塞、不能使用 RTOS API。
     */
    static bool Lcd_StartNextDmaChunk(void)
    {
        /* pixel_count 是本轮复制/发送的像素数量，最大 256，以匹配 lcd_dma_transfer_buffer 的固定容量。 */
        uint16_t pixel_count;

        if (lcd_dma_remaining_pixels == 0U) {
            return false;
        }

        if (lcd_dma_remaining_pixels > LCD_FILL_CHUNK_PIXELS) {
            pixel_count = LCD_FILL_CHUNK_PIXELS;
        } else {
            pixel_count = (uint16_t)lcd_dma_remaining_pixels;
        }

        /*
         * index 是当前 DMA 分块内的像素下标；每个 uint16_t RGB565 都拆为 ST7789 所需的高字节、低字节。
         * 这里沿用 V1 已实物验证的转换，DMA 只改变字节如何搬到 SPI，不改变颜色格式或字节序。
         */
        for (uint16_t index = 0U; index < pixel_count; ++index) {
            /* color 是源数组中当前 RGB565 像素，读取方是驱动，调用者在事务结束前不得改写它。 */
            const uint16_t color = lcd_dma_current_pixel[index];

            lcd_dma_transfer_buffer[index * 2U] = (uint8_t)(color >> 8U);
            lcd_dma_transfer_buffer[index * 2U + 1U] = (uint8_t)color;
        }

        /*
         * 在启动 DMA 前推进帧进度：若中断在 HAL_SPI_Transmit_DMA() 返回后立刻到来，回调可直接准备下一块。
         * current_pixel 的单位是 uint16_t 元素，remaining_pixels 的单位是像素；两者每轮都减少 pixel_count。
         */
        lcd_dma_current_pixel += pixel_count;
        lcd_dma_remaining_pixels -= pixel_count;

        /*
         * HAL_SPI_Transmit_DMA(句柄, 字节缓冲区, 字节数) 让 DMA2 把本块 RAM 数据写入 SPI1 数据寄存器。
         * &hspi1 是 CubeMX 已连接 hdma_spi1_tx 的 SPI1 句柄；缓冲区是 static 且在事务结束前不可改写；
         * pixel_count * 2U 是 RGB565 的字节数。HAL_OK 仅表示异步传输成功启动，真正结束由 HAL_SPI_TxCpltCallback() 报告。
         */
        return HAL_SPI_Transmit_DMA(&hspi1, lcd_dma_transfer_buffer,
                                    (uint16_t)(pixel_count * 2U)) == HAL_OK;
    }

    bool Lcd_StartWritePixelsDma(uint16_t x, uint16_t y,
                                 uint16_t width, uint16_t height,
                                 const uint16_t *pixels)
    {
        if ((pixels == NULL) || !Lcd_IsRectValid(x, y, width, height) || lcd_dma_busy) {
            return false;
        }

        /* 先锁定 DMA 所有权，保证从设置窗口到启动第一块期间没有第二个调用者插入事务。 */
        lcd_dma_busy = true;
        /* 新帧从“未失败”状态开始；只有启动失败或错误回调才会改为 true。 */
        lcd_dma_failed = false;
        /* 保存调用者数组首地址；它在整个异步事务中必须保持有效且内容不变。 */
        lcd_dma_current_pixel = pixels;
        /* width * height 是本帧总像素数，使用 uint32_t 防止未来更大窗口的乘法溢出。 */
        lcd_dma_remaining_pixels = (uint32_t)width * height;

        /* 仍复用 V1 已验证的 0x2A/0x2B/0x2C 地址窗口与 Y 偏移逻辑。 */
        Lcd_SetAddressWindow(x, y, x + width - 1U, y + height - 1U);
        /* 将 CS 保持为低，确保所有 DMA 分块属于同一次 0x2C 像素数据事务。 */
        HAL_GPIO_WritePin(LCD_CS_GPIO_Port, LCD_CS_Pin, GPIO_PIN_RESET);
        /* DC=1 让 ST7789 把 DMA 字节解释为 RGB565 像素，而不是命令或地址参数。 */
        HAL_GPIO_WritePin(LCD_DC_GPIO_Port, LCD_DC_Pin, GPIO_PIN_SET);

        if (!Lcd_StartNextDmaChunk()) {
            /* HAL 拒绝首块时不会有完成中断，必须同步清理片选和所有权。 */
            Lcd_FinishDmaTransfer(true);
            return false;
        }

        return true;
    }

    bool Lcd_IsDmaBusy(void)
    {
        /* 读取 volatile 状态，得到主程序此刻是否仍必须保留 DMA 源数据。 */
        return lcd_dma_busy;
    }

    bool Lcd_DmaTransferFailed(void)
    {
        /* 读取最近一帧的 volatile 结果；应在 Lcd_IsDmaBusy() 为 false 后解释它。 */
        return lcd_dma_failed;
    }

    /*
     * HAL 在 hspi 指向的 SPI DMA 发送完成后调用此回调，调用上下文是 DMA2_Stream2 中断而不是主循环。
     * 只处理 APP2 的 hspi1，避免未来其他 SPI 外设触发回调时误释放 LCD 的 CS 或状态。
     */
    void HAL_SPI_TxCpltCallback(SPI_HandleTypeDef *hspi)
    {
        if ((hspi != &hspi1) || !lcd_dma_busy) {
            return;
        }

        if (lcd_dma_remaining_pixels > 0U) {
            /* 当前块完成但同一帧仍有像素；立刻准备下一块，CS 和 DC 保持原样。 */
            if (!Lcd_StartNextDmaChunk()) {
                /* 后续块启动失败时没有可靠的完成通知，统一按错误路径归还资源。 */
                Lcd_FinishDmaTransfer(true);
            }
        } else {
            /* 最后一块已由 HAL 确认完成，至此再结束 CS 事务并释放调用者源数据。 */
            Lcd_FinishDmaTransfer(false);
        }
    }

    /*
     * HAL 在 SPI/DMA 传输发生硬件错误时调用此回调，仍运行在中断上下文。
     * 当前阶段不在此调用 Error_Handler()：先释放 CS 和 busy，保留状态给调试器/上层查询，避免永远占住 LCD。
     */
    void HAL_SPI_ErrorCallback(SPI_HandleTypeDef *hspi)
    {
        if ((hspi == &hspi1) && lcd_dma_busy) {
            Lcd_FinishDmaTransfer(true);
        }
    }

    void Lcd_FillRect(uint16_t x, uint16_t y,
                      uint16_t width, uint16_t height,
                      uint16_t color)
    {
        /*
         * 使用静态小缓冲区而不是按整个矩形分配：整屏需要 134400 字节，
         * 超出栈空间；这里反复发送同一块 256 像素的颜色数据即可。
         */
        /* pixel_buffer 保存一段重复颜色的 SPI 字节流；每个 RGB565 像素占两个字节。 */
        static uint8_t pixel_buffer[LCD_FILL_CHUNK_PIXELS * 2U];
        /* remaining_pixels 是当前矩形尚未写入的像素数，不是字节数，故使用 uint32_t 防止面积计算溢出。 */
        uint32_t remaining_pixels;
        /* pixel_count 是本轮实际发送的像素数，最大为 256；发送字节数等于它乘以 2。 */
        uint16_t pixel_count;

        if (!Lcd_IsRectValid(x, y, width, height)) {
            return;
        }

        /* ST7789 要求 RGB565 高字节先通过 SPI 发出。 */
        /* index 是当前重复颜色块中的像素下标，范围为 0 到 255。 */
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
            /* 本轮已发送 pixel_count 个像素；减法后 while 条件决定是否需要发送下一块。 */
            remaining_pixels -= pixel_count;
        }
    }

    bool Lcd_WritePixels(uint16_t x, uint16_t y,
                         uint16_t width, uint16_t height,
                         const uint16_t *pixels)
    {
        /*
         * STM32F4 以小端方式存放 uint16_t：例如红色 0xF800 在内存中是
         * 0x00、0xF8；ST7789 却要求 SPI 先收到 0xF8、再收到 0x00。
         * 因此不能把 uint16_t 数组直接强制转换为 uint8_t 指针后发送。
         */
        /* transfer_buffer 是本轮 SPI 字节流；它把原始 uint16_t 像素转换为高字节在前的格式。 */
        static uint8_t transfer_buffer[LCD_FILL_CHUNK_PIXELS * 2U];
        /* current_pixel 指向本轮尚未转换的第一个 RGB565 像素；每轮发送后向前移动 pixel_count 个元素。 */
        const uint16_t *current_pixel = pixels;
        /* remaining_pixels 记录窗口中还没写入 LCD 的像素数量，初值为 width 乘以 height。 */
        uint32_t remaining_pixels;
        /* pixel_count 记录本轮要转换和发送的像素数，限制为 256 以匹配 transfer_buffer 容量。 */
        uint16_t pixel_count;

        if ((pixels == NULL) || !Lcd_IsRectValid(x, y, width, height)) {
            return false;
        }

        Lcd_SetAddressWindow(x, y, x + width - 1U, y + height - 1U);
        remaining_pixels = (uint32_t)width * height;

        while (remaining_pixels > 0U) {
            if (remaining_pixels > LCD_FILL_CHUNK_PIXELS) {
                pixel_count = LCD_FILL_CHUNK_PIXELS;
            } else {
                pixel_count = (uint16_t)remaining_pixels;
            }

            /* 每次把一段像素转换为 LCD 所需的高字节在前格式。 */
            /* index 是本传输块内的像素下标；current_pixel[index] 仍是 MCU 内存中的 uint16_t 颜色值。 */
            for (uint16_t index = 0U; index < pixel_count; ++index) {
                /* color 是当前待转换的 RGB565 值；它在下一行被拆成高字节和低字节。 */
                const uint16_t color = current_pixel[index];

                transfer_buffer[index * 2U] = (uint8_t)(color >> 8U);
                transfer_buffer[index * 2U + 1U] = (uint8_t)color;
            }

            Lcd_WriteData(transfer_buffer, pixel_count * 2U);
            /* 指针按像素元素前进，remaining_pixels 按像素数量递减；两者始终保持同一进度。 */
            current_pixel += pixel_count;
            remaining_pixels -= pixel_count;
        }

        return true;
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
            /* madctl 是 0x36 扫描方向寄存器的参数；0x00 保持当前已实测成功的竖屏方向。 */
            static const uint8_t madctl[] = {0x00U};
            /* colmod 是 0x3A 像素格式寄存器的参数；0x05 选择 RGB565，使一个像素固定占 16 bit。 */
            static const uint8_t colmod[] = {0x05U};

            /* b2 是 0xB2 前后肩时序参数；保持参考值，避免改变液晶行扫描时序。 */
            static const uint8_t b2[] = {0x0CU, 0x0CU, 0x00U, 0x33U, 0x33U};
            /* b7 是 0xB7 栅极控制参数，影响面板行驱动方式。 */
            static const uint8_t b7[] = {0x35U};
            /* bb 是 0xBB VCOM 电压参数，影响液晶对比度与稳定性。 */
            static const uint8_t bb[] = {0x19U};
            /* c0 是 0xC0 LCM 控制参数，描述面板驱动模式。 */
            static const uint8_t c0[] = {0x2CU};
            /* c2 是 0xC2 电压寄存器使能参数，必须先于 VRH/VDV 设置写入。 */
            static const uint8_t c2[] = {0x01U};
            /* c3 是 0xC3 VRH 电压参数，影响内部参考高电压。 */
            static const uint8_t c3[] = {0x12U};
            /* c4 是 0xC4 VDV 电压参数，配合 VRH 设置驱动电压。 */
            static const uint8_t c4[] = {0x20U};
            /* c6 是 0xC6 帧率参数，决定面板刷新时序。 */
            static const uint8_t c6[] = {0x0FU};
            /* d0 是 0xD0 电源控制参数，配置内部升压与供电行为。 */
            static const uint8_t d0[] = {0xA4U, 0xA1U};
            /* e0 是 0xE0 正 Gamma 曲线参数，描述较亮灰阶的亮度响应；它不是待显示的像素数据。 */
            static const uint8_t e0[] = {
                0xD0U, 0x04U, 0x0DU, 0x11U, 0x13U, 0x2BU, 0x3FU,
                0x54U, 0x4CU, 0x18U, 0x0DU, 0x0BU, 0x1FU, 0x23U
            };
            /* e1 是 0xE1 负 Gamma 曲线参数，补偿另一方向的灰阶响应；需与 e0 一起保持参考值。 */
            static const uint8_t e1[] = {
                0xD0U, 0x04U, 0x0CU, 0x11U, 0x13U, 0x2CU, 0x3FU,
                0x44U, 0x51U, 0x2FU, 0x1FU, 0x1FU, 0x20U, 0x23U
            };

            /* 先硬件复位，保证下面所有寄存器配置从已知面板状态开始。 */
            Lcd_Reset();

            /* 0x11 退出睡眠后，控制器要求至少等待 120 ms。 */
            Lcd_WriteCommand(0x11U);
            /* HAL_Delay 使用启动阶段已经可用的 HAL tick；此时调度器未运行，不能改用 osDelay。 */
            HAL_Delay(120U);

            /* 将上方每个配置数组按对应命令写入；数组名表达“参数是什么”，命令号表达“写到哪个寄存器”。 */
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
