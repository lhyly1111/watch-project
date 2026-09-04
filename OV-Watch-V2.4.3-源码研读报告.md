# OV-Watch V2.4.3 源码研读报告

## 目的与范围

本报告对应本地只读快照 `项目源码`，用于在从零复刻过程中定位架构、接口和固件边界。它不代表已经完成编译、烧录或实物验证。分析对象为压缩包内的 V2.4.3 源码、两个独立提供的固件文件及其副本。

## 文件身份与完整性

| 文件 | 格式与用途 | SHA-256 | 结论 |
| --- | --- | --- | --- |
| `BootLoader_F411.hex` | Intel HEX，SWD 烧录的 BootLoader 映像 | `A339984F94215C64500308C7DF32091FED82CB5F71DDBBCA3B8FE20719607B88` | 数据记录范围为 `0x08000000` 至 `0x08006EAF`，属于 Flash 起始区的 BootLoader。 |
| `OV_Watch_V2_4_3.bin` | 原始 APP 二进制，供 BootLoader 的 YMODEM 接收 | `A493ABB1ACE544E59D9B1674C9AFC7439327262739B017C234791C1DFCCBBC87` | 与压缩包 `Firmware/` 内同名 BIN 完全一致；首个向量表的 MSP 为 `0x20019888`，复位向量为 `0x0800C271`。 |
| `OV-Watch-2.4.3.zip` | V2.4.3 发布源码、硬件资料、固件与仿真器 | `12E82F8D1A3B745995308CE02CC264A8B1CF038443435499A57A0DACE6A053F9` | 已解压为 `项目源码/OV-Watch-2.4.3`；包含 `Software`、`Firmware`、`Hardware`、`lv_sim_vscode_win`。 |

压缩包内的 `OV_Watch_V2_4_3.bin` 与独立文件哈希相同。压缩包内的 `BootLoader_F411.hex` 哈希为 `85FBF4D4272BCF2036F66FA344564CA19B73581F45596D7410B5977435AE6599`，与独立提供的 HEX 不同。不能仅凭文件名把外部 HEX 与压缩包 APP 当成已验证的同一构建对。

## 两个固件分别是什么

`BootLoader_F411.hex` 是带地址的 Intel HEX 文件。BootLoader 从 Flash 基址运行，开机按住 KEY1 时进入蓝牙升级菜单；它通过 UART 蓝牙 SPP 使用 YMODEM 接收 APP，并把 APP 写入 `0x0800C000`。

`OV_Watch_V2_4_3.bin` 是不含地址信息的原始字节流。它不能像 HEX 一样由工具凭文件自身推断写入位置；本项目约定由 BootLoader 写到 `0x0800C000`。其复位向量 `0x0800C271` 是对此约定的独立静态佐证。BootLoader 还在 `0x08008000` 检查字符串 `APP FLAG`，确认后设置 MSP 并跳转到 APP。不要把 BIN 直接按普通单镜像方式烧到 `0x08000000`。

```text
0x08000000 - 0x08007FFF   BootLoader 运行区
0x08008000 - 0x0800BFFF   APP FLAG 区
0x0800C000 - 0x0807FFFF   APP 区
```

## 技术栈

- MCU 与底层：STM32F411CEU6、CMSIS、STM32CubeMX 6.9.2 生成的 HAL 框架；时钟为 HSI PLL 100 MHz，RTC 使用 LSE。
- RTOS：FreeRTOS，通过 CMSIS-RTOS2 API 创建任务、消息队列和软件定时器；少量初始化路径直接使用原生 FreeRTOS API。
- 图形：LVGL 8.2，ST7789 240 x 280 LCD，CST816 触摸输入；显示传输使用 SPI1 + DMA，背光使用 TIM3 PWM。
- 外设与通信：USART1 + DMA/IDLE 接收蓝牙串口，ADC 电池采样，RTC，软件 I2C，以及 EEPROM、MPU6050、AHT21、SPL06、LSM303DLH、EM7028 等 BSP。
- PC 仿真：`lv_sim_vscode_win` 提供 CMake、Makefile、LVGL 与 SDL2 仿真工程。它是 UI 学习入口，不是 STM32 APP 的构建入口。
- 原始嵌入式构建：APP 与 IAP 均提供 Keil MDK ARMCC 5 工程；本地 V2.4.3 源码没有嵌入式 APP/IAP 的 CMake 构建入口。若改用 CLion，必须在独立复刻工程中建立 CMake、Arm GNU Toolchain、链接脚本和 OpenOCD/ST-LINK 配置，不能在参考快照内生成文件。

## 软件框架

```text
复位
  -> BootLoader: KEY1 判定 -> 蓝牙 YMODEM 升级，或检查 APP FLAG 后跳转 APP
  -> APP main.c: HAL/时钟/CubeMX 外设初始化，VTOR = 0x0000C000
  -> osKernelInitialize() -> MX_FREERTOS_Init() -> User_Tasks_Init()
  -> osKernelStart()
  -> HardwareInitTask: BSP、传感器、EEPROM、蓝牙、触摸、LCD、LVGL/UI 初始化后删除自身
  -> 常驻任务协作处理输入、页面、数据、通信、存储和低功耗
```

`User_Tasks_Init()` 是任务登记中心。主要任务包括：`LvHandlerTask` 运行 `lv_task_handler()`；`KeyTask` 扫描按键并投递队列；`ScrRenewTask` 消费按键消息、控制页面返回和传感器休眠；传感器、心率、充电检测、蓝牙消息、数据保存、看门狗与低功耗任务分别处理各自责任。

队列用于“发生一次”的通知，例如按键、熄屏、进入 STOP、首页刷新与保存请求。大多容量很小，因此它们不是高频无损数据管道。`TaskTickHook()` 以 1 ms 推进 LVGL 时基；其中不能阻塞或执行长操作。

## UI 与硬件的边界

`User/Func/HWDataAccess.h` 是重要边界：实机工程 `HW_USE_HARDWARE=1`，UI 通过 `HWInterface` 读取 RTC、BLE、电源、IMU、环境和心率数据；仿真工程可以把同一抽象切为 `0`，以模拟数据替代硬件。`PageManager.c` 用页面栈管理加载、返回与回到首页，屏幕文件位于 `User/GUI_App`。

因此，从零复刻的可控路径是：先在 PC 仿真器理解和验证 UI，再在独立 STM32 工程逐层补齐硬件抽象、BSP 和 FreeRTOS 任务。不要把 UI 页面显示成功误判为全部传感器、OTA 或低功耗已经可靠。

## 已知静态风险

- V2.4.3 本地快照与当前 GitHub `main` 的 V2.4.5 参考不一致；后续讨论必须明确所用版本，不能混合文件。
- 本地 APP 源码中的 LCD/触摸引脚定义仍需与手头 Core 板原理图和实物波形核对；静态源码不能替代板级验证。
- BootLoader 的界面字符串显示 V2.4.1，独立 HEX 又与压缩包 HEX 不同。烧录前应以哈希、链接地址、实物恢复路径和实际 BSP 引脚为依据，而不是只看文件名。
- 本报告只完成静态研读和归档校验，未执行 Keil/CLion 编译、SWD 烧录、蓝牙 OTA、外设通信或功耗测试。

## 后续阅读顺序

1. `Core/Src/main.c`、`freertos.c`、`User/Tasks/Src/user_TasksInit.c`、`user_HardwareInitTask.c`：启动与初始化。
2. `user_KeyTask.c`、`user_ScrRenewTask.c`、`User/Func/Src/PageManager.c`：输入到页面切换。
3. `HWDataAccess.*` 与 `lv_sim_vscode_win/user_test`：UI 与硬件抽象。
4. `BSP/LCD`、`BSP/TOUCH`、`BSP/IIC`：先做最小显示和触摸验证。
5. `Software/IAP_F411`、`Ymodem/flash_if.h`：最后再研究 Flash 分区和 OTA。
