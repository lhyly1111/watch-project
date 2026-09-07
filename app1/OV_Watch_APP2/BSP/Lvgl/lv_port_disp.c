/*
 * LVGL 到 APP2 LCD BSP 的阻塞式显示端口。
 * 数据流：LVGL 绘制 RGB565 缓冲区 -> lv_port_disp_flush() -> Lcd_WritePixels() -> ST7789。
 * 当前版本故意不启动 DMA：Lcd_WritePixels() 返回时像素已经发送完，才允许调用 lv_disp_flush_ready()。
 * 后续 DMA 版只能替换完成通知的位置，不能改变 LVGL 面积坐标和颜色数据的含义。
 */
#include "lv_port_disp.h"

#include <stdint.h>

#include "bsp_lcd.h"
#include "lvgl.h"
#include "main.h"

/* LCD 的逻辑可见宽度，单位为像素；应与 BSP/Lcd 当前已经实物验证的 240 列保持一致。 */
#define LV_PORT_DISP_HOR_RES 240U
/* LCD 的逻辑可见高度，单位为像素；应与 BSP/Lcd 当前已经实物验证的 280 行保持一致。 */
#define LV_PORT_DISP_VER_RES 280U
/*
 * 每个 LVGL 绘制缓冲区包含的完整行数。20 行使缓冲区为 240 * 20 * 2 = 9600 B，
 * 远小于会占用 134400 B 的全屏缓冲区；它由 LVGL 按需要分段刷新，不要求每次刷新整屏。
 */
#define LV_PORT_DISP_BUFFER_LINES 20U
/* 缓冲区总像素数；LVGL 的 lv_disp_draw_buf_init() 按“像素个数”而非字节数接收该值。 */
#define LV_PORT_DISP_BUFFER_PIXELS (LV_PORT_DISP_HOR_RES * LV_PORT_DISP_BUFFER_LINES)

/*
 * lv_port_draw_buffer 是 LVGL 软件绘制后交给 flush_cb 的 RGB565 像素区。
 * 它是 static，生命周期覆盖整个固件运行；LVGL 写入它，阻塞式 Lcd_WritePixels() 读取它。
 * 未来 DMA 传输期间该数组不能被 LVGL 覆盖，届时需要把 flush_ready 延后到 DMA 最终完成回调。
 */
static lv_color_t lv_port_draw_buffer[LV_PORT_DISP_BUFFER_PIXELS];
/*
 * lv_port_draw_buf 是 LVGL 对上方数组的描述符，记录单/双缓冲地址及像素容量。
 * 它只在 LvPortDisp_Init() 写入一次，随后由 LVGL 内部在每轮刷新时读取，故必须是 static。
 */
static lv_disp_draw_buf_t lv_port_draw_buf;
/*
 * lv_port_disp_drv 是 LVGL 显示驱动描述符，保存分辨率、draw buffer 与 flush_cb。
 * 初始化并注册后 LVGL 会继续持有它的内容，因此不能定义为 LvPortDisp_Init() 的局部变量。
 */
static lv_disp_drv_t lv_port_disp_drv;
/*
 * lv_port_flush_failed 仅在阻塞 LCD 写入因参数或 SPI 故障无法完成时置 true，调试器可读取。
 * 正常路径只在 LvPortDisp_Init() 清零；失败后 Error_Handler() 停止运行，不允许 LVGL 错误复用未显示的数据。
 */
static volatile bool lv_port_flush_failed;

/* LVGL 调用本函数提交一个需要刷新的包含式矩形；实现见下方。 */
static void LvPortDisp_Flush(lv_disp_drv_t *disp_drv,
                             const lv_area_t *area,
                             lv_color_t *color_p);

/*
 * 建立 LVGL 显示设备与 APP2 LCD 的对应关系。
 * 输入依赖：lv_init() 已完成、Lcd_Init() 已完成。输出：LVGL 此后会通过 LvPortDisp_Flush() 请求显示刷新。
 * 本函数不发像素、不创建控件；因此返回成功不能证明 LCD 画面，仍需由后续页面和实物验证取证。
 */
void LvPortDisp_Init(void)
{
    /*
     * C11 静态断言：APP2 配置中的 lv_color_t 必须正好是一个 16-bit RGB565 值。
     * 若未来改成 32-bit LVGL 颜色，编译会在这里失败，而不会把错误格式交给 Lcd_WritePixels()。
     */
    _Static_assert(sizeof(lv_color_t) == sizeof(uint16_t),
                   "APP2 LCD port requires 16-bit LVGL RGB565 colors");

    /* 每次端口初始化时清除旧错误标记；当前启动流程只调用一次，写入者仅为本函数和失败分支。 */
    lv_port_flush_failed = false;

    /*
     * lv_disp_draw_buf_init(描述符, 缓冲区1, 缓冲区2, 像素数) 告诉 LVGL 可以在哪里进行软件绘制。
     * &lv_port_draw_buf 持久保存配置；lv_port_draw_buffer 是单缓冲首地址；NULL 明确没有第二缓冲；
     * LV_PORT_DISP_BUFFER_PIXELS=4800 是元素数量而非 9600 B。单缓冲保证当前阻塞后端不会并发复用像素区。
     */
    lv_disp_draw_buf_init(&lv_port_draw_buf, lv_port_draw_buffer, NULL,
                          LV_PORT_DISP_BUFFER_PIXELS);

    /* lv_disp_drv_init() 为驱动描述符写入 LVGL 的安全默认值，必须先调用再覆盖 APP2 的字段。 */
    lv_disp_drv_init(&lv_port_disp_drv);
    /* hor_res/ver_res 是逻辑坐标范围；LVGL 之后给 flush_cb 的 area 必须落在 0..239、0..279。 */
    lv_port_disp_drv.hor_res = LV_PORT_DISP_HOR_RES;
    lv_port_disp_drv.ver_res = LV_PORT_DISP_VER_RES;
    /* draw_buf 指向上述单缓冲描述符，LVGL 据此轮流渲染并提交局部矩形。 */
    lv_port_disp_drv.draw_buf = &lv_port_draw_buf;
    /* flush_cb 是 LVGL 与硬件显示之间唯一的像素出口；不能遗漏，否则对象会绘制在 RAM 中却永远不显示。 */
    lv_port_disp_drv.flush_cb = LvPortDisp_Flush;

    /*
     * lv_disp_drv_register() 将描述符注册进 LVGL，并创建内部的周期刷新计时器。
     * 此调用只登记回调和分辨率，不会自行证明 LCD 工作；必须由 lv_timer_handler() 后续驱动刷新。
     */
    (void)lv_disp_drv_register(&lv_port_disp_drv);
}

/*
 * 将 LVGL 的一块包含式 RGB565 区域同步写入 LCD，并归还绘制缓冲区。
 * area 的 x2/y2 是最后一个仍属于区域的坐标，故宽高必须各加 1；color_p 的布局是逐行连续、
 * 总元素数为 width * height。阻塞写入结束前不得调用 lv_disp_flush_ready()，否则 LVGL 可能改写 color_p。
 */
static void LvPortDisp_Flush(lv_disp_drv_t *disp_drv,
                             const lv_area_t *area,
                             lv_color_t *color_p)
{
    /* width 是本次包含式 area 的列数，范围至少为 1；用 int32_t 先计算避免坐标差在窄类型中失真。 */
    const int32_t width = area->x2 - area->x1 + 1;
    /* height 是本次包含式 area 的行数，范围至少为 1；它决定 color_p 中包含多少行。 */
    const int32_t height = area->y2 - area->y1 + 1;

    /*
     * Lcd_WritePixels(x, y, width, height, pixels) 是已实物验证的同步 RGB565 BSP 接口。
     * area 起点映射为逻辑 LCD 起点；width/height 已由包含式终点换算；color_p 的 16-bit layout
     * 已由静态断言约束为 uint16_t RGB565。true 仅说明 STM32 SPI 写入完成，实物显示仍需下一步验证。
     */
    if (!Lcd_WritePixels((uint16_t)area->x1, (uint16_t)area->y1,
                         (uint16_t)width, (uint16_t)height,
                         (const uint16_t *)color_p))
    {
        /* 参数异常或 SPI 故障时不应宣称刷新完成；保留标记后停止，供调试器定位失败时的 area/状态。 */
        lv_port_flush_failed = true;
        Error_Handler();
    }

    /*
     * lv_disp_flush_ready() 告诉 LVGL color_p 已不再被 LCD BSP 读取，可以绘制下一块。
     * 该调用必须在阻塞 Lcd_WritePixels() 成功返回之后；DMA 版本会把这一句移动到最终完成回调。
     */
    lv_disp_flush_ready(disp_drv);
}
