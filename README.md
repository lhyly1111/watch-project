# OV Watch APP2

这是 OV-Watch 手表的独立复刻学习工程，当前实现目录为 `app1/OV_Watch_APP2`。

## 工程定位

- MCU：STM32F411CEU6，UFQFPN48
- 工具链：STM32CubeMX 生成基础框架，CLion + CMake + Arm GNU Toolchain 构建和调试
- 实时系统：FreeRTOS，通过 CMSIS-RTOS2 接口创建任务
- 当前目标：先完成最小硬件闭环，再逐步接入 DMA、LVGL、触摸和传感器
- 只读参考：`项目源码/OV-Watch-2.4.3`，用于对照，不在其中修改或构建

## 当前代码状态

| 模块 | 当前状态 | 验证状态 |
| --- | --- | --- |
| TIM3 / PB0 背光 | `BSP/Backlight` 已实现 | 已在实物板验证亮度变化 |
| ADC1 / PA1 电池采样 | `BSP/Battery` 已实现单次和 8 次平均读取 | 代码和调试器已验证，等待万用表校准 |
| PA4 / PA5 按键 | `BSP/Key` 轮询接口已实现 | 已编译；当前实物板没有对应实体按键 |
| SPI1 显示接口 | PB3 SCK、PB5 MOSI，Mode 3，当前 12.5 MHz | 已编译，尚未完成首屏验证 |
| LCD 初始化 | `BSP/Lcd/bsp_lcd.c` 已实现复位和 ST7789 初始化表 | 已编译，尚未由 `main.c` 调用 |

## 2026-09-06 学习总结

今天完成了从 CubeMX 外设骨架到最小 LCD BSP 的学习闭环设计。

### 已掌握

1. 区分旧的 `OV_Watch_app1` 和当前的 `OV_Watch_APP2`。前者是 STM32F411RET6 的早期骨架，不能作为手表工程入口；后者使用与手表匹配的 STM32F411CEU6。
2. 理解 BSP 分层：CubeMX 负责芯片初始化和引脚生成，自写硬件模块放在 `BSP` 目录，并在顶层 `CMakeLists.txt` 中显式登记。
3. 理解 LCD 的三根控制线：`CS` 选择面板，`DC=0` 表示命令，`DC=1` 表示参数或像素数据，`RST` 负责硬件复位。
4. 理解 `HAL_Delay()` 与 `osDelay()` 的使用边界：LCD 初始化发生在 `osKernelStart()` 之前，因此使用 `HAL_Delay()`。
5. 理解显示链路：`SPI/GPIO 初始化 -> 背光 -> LCD 复位 -> ST7789 初始化 -> 地址窗口 -> RGB565 像素 -> 屏幕图像`。

### 已验证与未验证

- APP2 Debug 构建通过，当前带 LCD BSP 的结果约为 Flash `24412 B`、RAM `20680 B`。
- 背光 `TIM3 CH3 -> PB0 -> LCD_BLK` 已在实物板验证。
- ADC 首次读数曾为 `raw=50`、约 `0.0806 V`；这证明读取路径执行，不证明电池分压网络正常。
- LCD 的 SPI 和初始化代码目前只完成编译验证；参考源码的 LCD 控制脚与 V2.4 图纸存在冲突，必须由首屏实物结果裁决。

### 下一步

在 `BSP/Lcd` 中实现地址窗口和轮询纯色填屏，随后在 `main.c` 中调用 `Lcd_Init()` 与 `Lcd_FillScreen(0xF800)`，先验证纯红，再验证绿、蓝、白。首屏成功前不引入 DMA、LVGL 或完整页面。

## 学习记录同步约定

每次完成一天的 OV-Watch 学习总结时：

1. 将当天已验证的 APP2 代码和配置整理为独立 Git 提交。
2. 把当天的学习总结、验证结果、未解决风险和下一步追加到本 `README.md`。
3. 排除 `build/`、`.idea/`、`tmp/`、固件输出和只读参考快照。
4. 构建并检查提交差异后，推送到 [lhyly1111/watch-project](https://github.com/lhyly1111/watch-project)。

详细的跨对话学习记录位于 `C:\Users\Administrator\Desktop\手表项目\OV-Watch-学习对话记录.md`。
