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
| SPI1 显示接口 | PB3 SCK、PB5 MOSI，Mode 3，12.5 MHz | 已由纯红首屏实测 |
| LCD 控制与首屏 | PB7 RST、PB8 CS、PB9 DC；初始化、地址窗口与 RGB565 填屏已接入 `main.c` | 纯红已在实物板验证 |

## 硬件调试原则

当 APP2 已构建、下载却没有预期硬件现象时，先用断点确认程序执行，再对照 `OV_Watch_APP2.ioc`、生成的 GPIO/SPI 配置与只读 V2.4.3 驱动。一次只改变一个引脚或协议参数，并以实物结果裁决配置是否正确。

## 学习笔记

每日学习总结保存在 Obsidian Vault：`C:\Users\Administrator\Desktop\Obsidian\OV_Watch\每日总结`。关系链保存在 `C:\Users\Administrator\Desktop\Obsidian\OV_Watch\关系链`，用于直观展示源码、CubeMX 配置、BSP、硬件验证与后续工作的依赖关系。

本仓库仅同步工程代码与配置；不再把每日学习总结写入 `README.md` 或上传至 GitHub。详细的跨对话学习记录位于 `C:\Users\Administrator\Desktop\手表项目\OV-Watch-学习对话记录.md`。
