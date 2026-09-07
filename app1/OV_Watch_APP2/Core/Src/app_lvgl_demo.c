/*
 * APP2 的最小 LVGL 页面：黑色背景、绿色状态块、英文标签和每秒变化的计数。
 * 目的不是做正式手表界面，而是用少量可辨认对象验证 LVGL 对象创建、颜色、文字、局部刷新和显示端口时序。
 */
#include "app_lvgl_demo.h"

#include <stdint.h>

#include "lvgl.h"

/*
 * lvgl_demo_counter_label 指向页面底部的计数标签；创建函数写入一次，1 秒定时器回调持续读取并更新它。
 * 该对象由 LVGL 内部内存池管理，在本阶段页面生命周期内有效；后续切换页面时应改为显式删除或重新创建。
 */
static lv_obj_t *lvgl_demo_counter_label;
/*
 * lvgl_demo_refresh_count 记录计数标签已更新的次数，单位为次，范围从 0 递增至 uint32_t 上限后回绕。
 * 它只由 LVGL 定时器回调写入，标签文本读取它；目前只作为周期局部刷新的可见证据，不是业务数据。
 */
static uint32_t lvgl_demo_refresh_count;

/* LVGL 每秒调用一次本回调，更新计数标签以强制产生一个可观测的小区域刷新。 */
static void AppLvglDemo_UpdateCounter(lv_timer_t *timer);

/*
 * 在当前活动屏幕上创建最小验证页面。
 * 输入依赖：LVGL 已初始化且显示端口已注册。输出：LVGL 对象树和 1 秒软件定时器；真正发像素由
 * lv_timer_handler() 间接触发。此函数只能由唯一 UI 上下文调用，避免与后续输入/传感器任务并发改对象。
 */
void AppLvglDemo_Create(void)
{
    /* screen 指向 LVGL 当前活动根对象；本页面把背景和所有子控件都挂在该根对象下。 */
    lv_obj_t *screen = lv_scr_act();
    /* status_block 是绿色矩形的对象指针；它同时作为两条状态标签的父对象，生命周期由 screen 管理。 */
    lv_obj_t *status_block;
    /* title_label 是状态块中的版本标签；只在创建时写入固定英文文本。 */
    lv_obj_t *title_label;
    /* detail_label 是状态块中的端口说明标签；用于验证第二行文字的坐标与颜色。 */
    lv_obj_t *detail_label;

    /* 从零开始计数，确保每次冷启动的可见刷新序列可预测；写入者是本函数和定时器回调。 */
    lvgl_demo_refresh_count = 0U;

    /* lv_obj_set_style_bg_color() 设置 screen 的默认背景颜色；lv_color_black() 生成 RGB565 黑色。 */
    lv_obj_set_style_bg_color(screen, lv_color_black(), LV_PART_MAIN);
    /* lv_obj_set_style_bg_opa() 把背景设为完全不透明，避免下层默认样式影响黑色基准。 */
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, LV_PART_MAIN);

    /* lv_obj_create(screen) 在活动屏幕下创建通用矩形对象；screen 是父对象，LVGL 负责其内存生命周期。 */
    status_block = lv_obj_create(screen);
    /* lv_obj_set_size(对象, 宽, 高) 以像素设置绿色块尺寸；192 x 72 留出四周黑色边界便于观察方向。 */
    lv_obj_set_size(status_block, 192, 72);
    /* lv_obj_align() 按父对象中心定位，再向上偏移 32 像素；不会改变对象尺寸。 */
    lv_obj_align(status_block, LV_ALIGN_CENTER, 0, -32);
    /* 绿色块使用纯 RGB 颜色；它与黑背景形成明显对比，验证 LVGL 到 ST7789 的 RGB565 传递。 */
    lv_obj_set_style_bg_color(status_block, lv_color_hex(0x00A86B), LV_PART_MAIN);
    /* 确保状态块本身完全不透明，避免使用未启用的复杂透明绘制路径。 */
    lv_obj_set_style_bg_opa(status_block, LV_OPA_COVER, LV_PART_MAIN);
    /* 边框宽度为 0，避免初版把边框、圆角等额外绘制变量混入显示端口验证。 */
    lv_obj_set_style_border_width(status_block, 0, LV_PART_MAIN);

    /* lv_label_create() 创建第一个文本对象，父对象为 status_block；它会跟随绿色块移动和释放。 */
    title_label = lv_label_create(status_block);
    /* 设置 ASCII 文本，避免当前未导入中文字体导致“端口错误”与“字库缺失”混淆。 */
    lv_label_set_text(title_label, "LVGL 8.2");
    /* 显式指定 14 px Montserrat 字体，与 APP2 lv_conf.h 中唯一启用的内置字体一致。 */
    lv_obj_set_style_text_font(title_label, &lv_font_montserrat_14, LV_PART_MAIN);
    /* 文字使用白色，验证同一绿色背景上的前景 RGB565 颜色。 */
    lv_obj_set_style_text_color(title_label, lv_color_white(), LV_PART_MAIN);
    /* 标题相对绿色块顶部居中；x/y 偏移为像素，正 y 方向向下。 */
    lv_obj_align(title_label, LV_ALIGN_TOP_MID, 0, 12);

    /* 第二个标签验证多个对象、两行不同位置文字均能经同一 flush_cb 正确刷新。 */
    detail_label = lv_label_create(status_block);
    lv_label_set_text(detail_label, "Blocking LCD port");
    lv_obj_set_style_text_font(detail_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(detail_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(detail_label, LV_ALIGN_BOTTOM_MID, 0, -12);

    /* 底部计数标签保存在 static 指针中，供定时器每秒局部更新；父对象为 screen。 */
    lvgl_demo_counter_label = lv_label_create(screen);
    /* 首次文本与计数变量对应，便于下载后立刻识别页面已被创建。 */
    lv_label_set_text(lvgl_demo_counter_label, "Refresh count: 0");
    lv_obj_set_style_text_font(lvgl_demo_counter_label, &lv_font_montserrat_14, LV_PART_MAIN);
    lv_obj_set_style_text_color(lvgl_demo_counter_label, lv_color_white(), LV_PART_MAIN);
    lv_obj_align(lvgl_demo_counter_label, LV_ALIGN_CENTER, 0, 54);

    /*
     * lv_timer_create(回调, 周期毫秒, 用户数据) 注册 LVGL 软件定时器；它不会开硬件中断，
     * 而是在唯一 UI 任务的 lv_timer_handler() 中执行。NULL 表示本回调不需要额外用户数据。
     * 返回的 lv_timer_t 由 LVGL 内部持有，本阶段无需保存；创建成功的可见证据是计数每秒变化。
     */
    (void)lv_timer_create(AppLvglDemo_UpdateCounter, 1000U, NULL);
}

/*
 * 将刷新次数加一并刷新底部标签。
 * timer 是 LVGL 传入的定时器实例；当前页面不用它，但显式忽略可说明并非遗漏参数。
 * 本回调运行在 lv_timer_handler() 所在的 UI 任务上下文，不是中断，允许安全修改 LVGL 对象。
 */
static void AppLvglDemo_UpdateCounter(lv_timer_t *timer)
{
    /* 本演示不读取定时器属性；保留参数是 LVGL 回调签名要求。 */
    (void)timer;

    /* 每次 1 秒到期后递增；该变化会让 LVGL 标记标签区域为 dirty，随后经 flush_cb 产生局部刷新。 */
    ++lvgl_demo_refresh_count;
    /* lv_label_set_text_fmt() 格式化并替换标签文本；%lu 与 unsigned long 强制转换匹配当前 32-bit MCU 的计数宽度。 */
    lv_label_set_text_fmt(lvgl_demo_counter_label, "Refresh count: %lu",
                          (unsigned long)lvgl_demo_refresh_count);
}
