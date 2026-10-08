/**
 * @file    cabinet_guard_ui.c
 * @brief   CabinetGuard 四页 HMI（替换旧 Machine Dashboard）
 *
 * 布局（800x480）：
 *   ┌─────────────────────────────────────┐
 *   │ Header（常驻）：标题 / 时钟 / 状态灯 │
 *   ├─────────────────────────────────────┤
 *   │ Content：当前仅 1 页（切页销毁再建） │
 *   ├─────────────────────────────────────┤
 *   │ Nav（常驻）：OVERVIEW|I/O|PARAM|SYS  │
 *   └─────────────────────────────────────┘
 *
 * 设计约束：
 * - 字体仅复用 montserrat 14/24/28，不加新字库/PNG/FS
 * - LV_MEM 紧张 → 懒加载：切页 lv_obj_clean 后重建，不全量常驻 4 页
 * - 显示为 PARTIAL 刷新：控件需不透明底，切页 invalidate，启动时 lcd_clear
 *
 * 数据：温湿度/水浸 ADC ← BSP_AppData；继电器/蜂鸣器 ← Modbus Coil
 * 入口：lvgl_demo 任务里 create + 500ms refresh
 */
#include "cabinet_guard_ui.h"

#include "bsp_app_data.h"
#include "bsp_modbus_app.h"
#include "lcd.h"
#include "lvgl.h"
#include "main.h"

#include <stdbool.h>
#include <stdint.h>
#include <string.h>

/* ---------- 布局尺寸 ---------- */
#define SCREEN_W   800
#define SCREEN_H   480
#define HEADER_H   58
#define NAV_H      72
#define CONTENT_H  (SCREEN_H - HEADER_H - NAV_H)

/* ---------- 主题色（RGB888，经 lv_color_hex） ---------- */
#define BG       0x03101E
#define PANEL    0x0A1D33
#define PANEL_2  0x0D243D
#define BORDER   0x264667
#define GREEN    0x20E89A
#define LIME     0x82DC48
#define ORANGE   0xFF9E2C
#define RED      0xFF3346
#define WHITE    0xF2F5FA
#define MUTED    0x9BACBF

/* 仅复用工程已启用字体（lv_conf 里打开的三档） */
#define FONT_S   (&lv_font_montserrat_14)
#define FONT_M   (&lv_font_montserrat_24)
#define FONT_L   (&lv_font_montserrat_28)

/*
 * 排查残影：仅显示 Overview，禁止切页。
 * 测完改回 0，恢复四页。
 */
#define CABINET_UI_OVERVIEW_ONLY  0

/** 底栏四页 ID，与 nav_buttons[] 下标一致 */
typedef enum {
    PAGE_OVERVIEW = 0,
    PAGE_IO,
    PAGE_PARAMETERS,
    PAGE_SYSTEM,
    PAGE_COUNT
} page_id_t;

/** Overview 页需周期性刷新的控件句柄（切页后由 clear_live_ptrs 清零） */
typedef struct {
    lv_obj_t *temp_value;
    lv_obj_t *hum_value;
    lv_obj_t *temp_status;
    lv_obj_t *hum_status;
    lv_obj_t *temp_arc;
    lv_obj_t *hum_arc;
    lv_obj_t *estop_icon;
    lv_obj_t *estop_state;
    lv_obj_t *leak_icon;
    lv_obj_t *leak_state;
    lv_obj_t *relay_sw;
    lv_obj_t *relay_label;
    lv_obj_t *buzzer_sw;
    lv_obj_t *buzzer_label;
} overview_live_t;

/** I/O 页需周期性刷新的控件句柄 */
typedef struct {
    lv_obj_t *relay_sw;
    lv_obj_t *relay_label;
    lv_obj_t *buzzer_sw;
    lv_obj_t *buzzer_label;
    lv_obj_t *water_icon;
    lv_obj_t *water_state;
} io_live_t;

/** 全局 UI 状态（单例） */
typedef struct {
    lv_obj_t *clock;                    /**< 顶栏时钟 */
    lv_obj_t *hdr_leds[5];              /**< ETH/MOD/OTA/E-STOP/WATER */
    lv_obj_t *content;                  /**< 中间内容区容器 */
    lv_obj_t *page;                     /**< 当前页根对象（切页 clean） */
    lv_obj_t *nav_buttons[PAGE_COUNT];  /**< 底栏四个按钮 */
    page_id_t active;                   /**< 当前页；PAGE_COUNT 表示未建 */
    overview_live_t ov;
    io_live_t io;
    bool wet_latched;                   /**< 水浸告警锁存（滞回） */
    bool syncing_coils;                 /**< 程序改开关时禁止回写 Coil */
} ui_t;

static ui_t s_ui;

/* ==========================================================================
 * 基础控件工厂
 * ========================================================================== */

static lv_color_t color(uint32_t hex)
{
    return lv_color_hex(hex);
}

/** 静态文案标签（透明底即可） */
static lv_obj_t *make_label(lv_obj_t *parent, const char *text, const lv_font_t *font, uint32_t hex)
{
    lv_obj_t *label = lv_label_create(parent);
    lv_label_set_text(label, text);
    lv_obj_set_style_text_font(label, font, 0);
    lv_obj_set_style_text_color(label, color(hex), 0);
    return label;
}

/**
 * 会周期性改字的标签：自带不透明 PANEL 底
 * PARTIAL 模式下纯透明标签改字易留下旧字形残影
 */
static lv_obj_t *make_live_label(lv_obj_t *parent, const char *text, const lv_font_t *font, uint32_t hex)
{
    lv_obj_t *label = make_label(parent, text, font, hex);
    lv_obj_set_style_bg_color(label, color(PANEL), 0);
    lv_obj_set_style_bg_opa(label, LV_OPA_COVER, 0);
    lv_obj_set_style_pad_hor(label, 2, 0);
    return label;
}

/** 卡片面板：纯色底；圆角为 0，避免 PARTIAL 切页时圆角外透出旧像素 */
static lv_obj_t *make_panel(lv_obj_t *parent, int x, int y, int w, int h)
{
    lv_obj_t *panel = lv_obj_create(parent);
    lv_obj_set_pos(panel, x, y);
    lv_obj_set_size(panel, w, h);
    lv_obj_set_style_bg_color(panel, color(PANEL), 0);
    lv_obj_set_style_bg_opa(panel, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(panel, color(BORDER), 0);
    lv_obj_set_style_border_width(panel, 1, 0);
    lv_obj_set_style_radius(panel, 0, 0);
    lv_obj_set_style_pad_all(panel, 10, 0);
    lv_obj_remove_flag(panel, LV_OBJ_FLAG_SCROLLABLE);
    return panel;
}

/** 1px 分隔线 */
static void add_line(lv_obj_t *parent, int x, int y, int w)
{
    lv_obj_t *line = lv_obj_create(parent);
    lv_obj_remove_style_all(line);
    lv_obj_set_pos(line, x, y);
    lv_obj_set_size(line, w, 1);
    lv_obj_set_style_bg_color(line, color(BORDER), 0);
    lv_obj_set_style_bg_opa(line, LV_OPA_COVER, 0);
}

/** 圆形状态灯（无阴影，避免 PARTIAL 花屏） */
static lv_obj_t *make_led(lv_obj_t *parent, int x, int y, uint32_t hex, bool on, int size)
{
    lv_obj_t *led = lv_obj_create(parent);
    lv_obj_remove_style_all(led);
    lv_obj_set_pos(led, x, y);
    lv_obj_set_size(led, size, size);
    lv_obj_set_style_radius(led, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(led, color(on ? hex : 0x26394D), 0);
    lv_obj_set_style_bg_opa(led, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(led, color(on ? hex : 0x40536A), 0);
    lv_obj_set_style_border_width(led, 1, 0);
    return led;
}

/**
 * @brief  设置界面 LED 指示状态
 */
static void set_led_state(lv_obj_t *led, uint32_t hex, bool on)
{
    if (led == NULL) {
        return;
    }
    lv_obj_set_style_bg_color(led, color(on ? hex : 0x26394D), 0);
    lv_obj_set_style_border_color(led, color(on ? hex : 0x40536A), 0);
}

/** Modbus 线圈开关外观 */
static lv_obj_t *make_switch(lv_obj_t *parent, int x, int y, bool on)
{
    lv_obj_t *sw = lv_switch_create(parent);
    lv_obj_set_pos(sw, x, y);
    lv_obj_set_size(sw, 52, 28);
    lv_obj_set_style_bg_color(sw, color(0x31465D), LV_PART_MAIN);
    lv_obj_set_style_bg_color(sw, color(GREEN), LV_PART_INDICATOR | LV_STATE_CHECKED);
    lv_obj_set_style_bg_color(sw, color(WHITE), LV_PART_KNOB);
    if (on) {
        lv_obj_add_state(sw, LV_STATE_CHECKED);
    }
    return sw;
}

/**
 * 半圆弧仪表（仅 lv_arc，不用 transform 指针）
 * 背景透明，避免方块底；依赖 LTDC FB 非 Cache 避免花屏
 */
static lv_obj_t *make_gauge(lv_obj_t *parent, int cx, int cy, int value, int max_v)
{
    lv_obj_t *arc = lv_arc_create(parent);
    lv_obj_set_size(arc, 92, 92);
    lv_obj_set_pos(arc, cx - 46, cy - 46);
    lv_arc_set_rotation(arc, 135);
    lv_arc_set_bg_angles(arc, 0, 270);
    lv_arc_set_range(arc, 0, max_v);
    lv_arc_set_value(arc, value);
    lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, color(0x1B334C), LV_PART_MAIN);
    lv_obj_set_style_arc_width(arc, 10, LV_PART_MAIN);
    lv_obj_set_style_arc_rounded(arc, false, LV_PART_MAIN);
    lv_obj_set_style_arc_color(arc, color(GREEN), LV_PART_INDICATOR);
    lv_obj_set_style_arc_width(arc, 10, LV_PART_INDICATOR);
    lv_obj_set_style_arc_rounded(arc, false, LV_PART_INDICATOR);
    lv_obj_set_style_bg_opa(arc, LV_OPA_TRANSP, LV_PART_KNOB);
    lv_obj_set_style_pad_all(arc, 0, LV_PART_KNOB);
    return arc;
}

/** 值未变则不刷新，减少弧形重绘 */
static void set_gauge(lv_obj_t *arc, int value)
{
    if (arc != NULL && lv_arc_get_value(arc) != value) {
        lv_arc_set_value(arc, value);
    }
}

/**
 * 圆形告警状态块：圆图标 + 标题 + 状态字
 * @param out_icon / out_state 可为 NULL（仅展示、不刷新）
 */
static void make_round_status(lv_obj_t *parent, int cx, int cy, uint32_t hex,
                              const char *symbol, const char *title, const char *state,
                              lv_obj_t **out_icon, lv_obj_t **out_state)
{
    lv_obj_t *circle = lv_obj_create(parent);
    lv_obj_remove_style_all(circle);
    lv_obj_set_size(circle, 70, 70);
    lv_obj_set_pos(circle, cx - 35, cy - 52);
    lv_obj_set_style_radius(circle, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(circle, color(hex), 0);
    lv_obj_set_style_bg_opa(circle, LV_OPA_COVER, 0);

    lv_obj_t *icon = make_label(circle, symbol, FONT_L, WHITE);
    lv_obj_center(icon);

    lv_obj_t *name = make_label(parent, title, FONT_S, WHITE);
    lv_obj_set_pos(name, cx - 40, cy + 28);
    lv_obj_t *st = make_live_label(parent, state, FONT_S, hex);
    lv_obj_set_pos(st, cx - 30, cy + 46);

    if (out_icon != NULL) {
        *out_icon = circle;
    }
    if (out_state != NULL) {
        *out_state = st;
    }
}

/** 刷新圆形告警块颜色 / 符号 / 状态文案 */
static void set_round_status(lv_obj_t *circle, lv_obj_t *state_label,
                             uint32_t hex, const char *symbol, const char *state_text)
{
    if (circle != NULL) {
        lv_obj_set_style_bg_color(circle, color(hex), 0);
        if (lv_obj_get_child_count(circle) > 0) {
            lv_obj_t *icon = lv_obj_get_child(circle, 0);
            lv_label_set_text(icon, symbol);
        }
    }
    if (state_label != NULL) {
        lv_label_set_text(state_label, state_text);
        lv_obj_set_style_text_color(state_label, color(hex), 0);
    }
}

/* ==========================================================================
 * Modbus Coil 开关绑定
 * ========================================================================== */

/** 用户拨动开关 → 写 Coil；syncing_coils 时忽略（避免 refresh 回写触发） */
static void coil_switch_cb(lv_event_t *e)
{
    lv_obj_t *sw;
    uint16_t coil;
    bool on;

    if (s_ui.syncing_coils) {
        return;
    }
    if (lv_event_get_code(e) != LV_EVENT_VALUE_CHANGED) {
        return;
    }

    sw = lv_event_get_target(e);
    coil = (uint16_t)(uintptr_t)lv_event_get_user_data(e);
    on = lv_obj_has_state(sw, LV_STATE_CHECKED);
    BSP_Modbus_SetCoil(coil, on ? 1U : 0U);
}

/** user_data 存放线圈号 */
static void bind_coil_switch(lv_obj_t *sw, uint16_t coil)
{
    lv_obj_add_event_cb(sw, coil_switch_cb, LV_EVENT_VALUE_CHANGED, (void *)(uintptr_t)coil);
}

/** 按 Coil 状态同步 UI 开关与 ON/OFF 文字（不触发写回） */
static void sync_switch_label(lv_obj_t *sw, lv_obj_t *label, bool on)
{
    s_ui.syncing_coils = true;
    if (sw != NULL) {
        if (on) {
            lv_obj_add_state(sw, LV_STATE_CHECKED);
        } else {
            lv_obj_remove_state(sw, LV_STATE_CHECKED);
        }
    }
    s_ui.syncing_coils = false;
    if (label != NULL) {
        lv_label_set_text(label, on ? "ON" : "OFF");
        lv_obj_set_style_text_color(label, color(on ? GREEN : MUTED), 0);
    }
}

/* ==========================================================================
 * 常驻：顶栏 / 底栏
 * ========================================================================== */

/** 顶栏：品牌 + 时钟 + 5 路状态灯 */
static void build_header(lv_obj_t *screen)
{
    lv_obj_t *header = lv_obj_create(screen);
    lv_obj_t *lab;
    const char *names[] = {"ETH", "MOD", "OTA", "E-STOP", "WATER"};
    const int xs[] = {465, 526, 587, 646, 724};
    uint8_t i;

    lv_obj_remove_style_all(header);
    lv_obj_set_pos(header, 0, 0);
    lv_obj_set_size(header, SCREEN_W, HEADER_H);
    lv_obj_set_style_bg_color(header, color(0x020C18), 0);
    lv_obj_set_style_bg_opa(header, LV_OPA_COVER, 0);
    lv_obj_set_style_border_color(header, color(0x1D354F), 0);
    lv_obj_set_style_border_width(header, 1, 0);
    lv_obj_set_style_border_side(header, LV_BORDER_SIDE_BOTTOM, 0);

    lab = make_label(header, LV_SYMBOL_HOME, FONT_L, GREEN);
    lv_obj_set_pos(lab, 16, 13);
    lab = make_label(header, "CabinetGuard", FONT_M, WHITE);
    lv_obj_set_pos(lab, 49, 9);
    lab = make_label(header, "CABINET MONITOR", FONT_S, MUTED);
    lv_obj_set_pos(lab, 50, 33);

    s_ui.clock = make_label(header, "--:--", FONT_L, WHITE);
    lv_obj_set_pos(s_ui.clock, 326, 13);

    for (i = 0; i < 5; i++) {
        s_ui.hdr_leds[i] = make_led(header, xs[i], 14, GREEN, true, 10);
        lab = make_label(header, names[i], FONT_S, WHITE);
        lv_obj_set_pos(lab, xs[i] - 7, 31);
    }
}

/* ==========================================================================
 * 各页 build（仅当前页存在于 s_ui.page 下）
 * ========================================================================== */

/**
 * Overview：环境仪表 | 告警 | 输出/远端 DI·DO
 * 温湿度、水浸、Coil0/1 由 refresh 更新；远端 DI/DO 灯暂占位
 */
static void build_overview(lv_obj_t *page)
{
    lv_obj_t *env = make_panel(page, 10, 8, 262, 334);
    lv_obj_t *alarms;
    lv_obj_t *outputs;
    lv_obj_t *lab;
    uint8_t i;

    lab = make_label(env, LV_SYMBOL_EYE_OPEN, FONT_M, GREEN);
    lv_obj_set_pos(lab, 6, 3);
    lab = make_label(env, "ENVIRONMENT", FONT_S, WHITE);
    lv_obj_set_pos(lab, 40, 8);
    add_line(env, 5, 34, 230);

    lab = make_label(env, "TEMPERATURE", FONT_S, MUTED);
    lv_obj_set_pos(lab, 8, 43);
    s_ui.ov.temp_value = make_live_label(env, "--", FONT_L, GREEN);
    lv_obj_set_pos(s_ui.ov.temp_value, 12, 65);
    lab = make_label(env, "C", FONT_S, WHITE);
    lv_obj_set_pos(lab, 91, 77);
    s_ui.ov.temp_arc = make_gauge(env, 187, 105, 0, 80);
    s_ui.ov.temp_status = make_live_label(env, "NORMAL", FONT_S, GREEN);
    lv_obj_set_pos(s_ui.ov.temp_status, 146, 156);
    add_line(env, 5, 180, 230);

    lab = make_label(env, "HUMIDITY", FONT_S, MUTED);
    lv_obj_set_pos(lab, 8, 190);
    s_ui.ov.hum_value = make_live_label(env, "--", FONT_L, GREEN);
    lv_obj_set_pos(s_ui.ov.hum_value, 20, 220);
    lab = make_label(env, "%RH", FONT_S, WHITE);
    lv_obj_set_pos(lab, 65, 231);
    s_ui.ov.hum_arc = make_gauge(env, 187, 259, 0, 100);
    s_ui.ov.hum_status = make_live_label(env, "NORMAL", FONT_S, GREEN);
    lv_obj_set_pos(s_ui.ov.hum_status, 146, 307);

    alarms = make_panel(page, 280, 8, 208, 334);
    lab = make_label(alarms, LV_SYMBOL_BELL, FONT_S, GREEN);
    lv_obj_set_pos(lab, 7, 5);
    lab = make_label(alarms, "ALARM STATUS", FONT_S, WHITE);
    lv_obj_set_pos(lab, 34, 6);
    add_line(alarms, 5, 34, 176);
    make_round_status(alarms, 94, 91, GREEN, LV_SYMBOL_OK, "E-STOP", "NORMAL",
                      &s_ui.ov.estop_icon, &s_ui.ov.estop_state);
    add_line(alarms, 5, 180, 176);
    make_round_status(alarms, 94, 231, GREEN, LV_SYMBOL_OK, "WATER LEAK", "NORMAL",
                      &s_ui.ov.leak_icon, &s_ui.ov.leak_state);

    outputs = make_panel(page, 496, 8, 294, 334);
    lab = make_label(outputs, LV_SYMBOL_SETTINGS, FONT_S, GREEN);
    lv_obj_set_pos(lab, 7, 5);
    lab = make_label(outputs, "OUTPUT CONTROL", FONT_S, WHITE);
    lv_obj_set_pos(lab, 34, 6);
    add_line(outputs, 5, 34, 262);

    lab = make_label(outputs, "RELAY", FONT_S, WHITE);
    lv_obj_set_pos(lab, 10, 48);
    s_ui.ov.relay_sw = make_switch(outputs, 198, 42, false);
    bind_coil_switch(s_ui.ov.relay_sw, MODBUS_COIL_LED_PA3);
    s_ui.ov.relay_label = make_live_label(outputs, "OFF", FONT_S, MUTED);
    lv_obj_set_pos(s_ui.ov.relay_label, 257, 49);

    lab = make_label(outputs, "BUZZER", FONT_S, WHITE);
    lv_obj_set_pos(lab, 10, 87);
    s_ui.ov.buzzer_sw = make_switch(outputs, 198, 81, false);
    bind_coil_switch(s_ui.ov.buzzer_sw, 1U);
    s_ui.ov.buzzer_label = make_live_label(outputs, "OFF", FONT_S, MUTED);
    lv_obj_set_pos(s_ui.ov.buzzer_label, 257, 88);

    add_line(outputs, 5, 122, 262);
    lab = make_label(outputs, "REMOTE DI / DO STATUS", FONT_S, WHITE);
    lv_obj_set_pos(lab, 5, 135);

    for (i = 0; i < 8; i++) {
        char n[5];
        lv_snprintf(n, sizeof(n), "DI%d", i + 1);
        lab = make_label(outputs, n, FONT_S, MUTED);
        lv_obj_set_pos(lab, 3 + i * 33, 166);
        make_led(outputs, 7 + i * 33, 187, GREEN, false, 16);
        lv_snprintf(n, sizeof(n), "DO%d", i + 1);
        lab = make_label(outputs, n, FONT_S, MUTED);
        lv_obj_set_pos(lab, 1 + i * 33, 229);
        make_led(outputs, 7 + i * 33, 250, GREEN, false, 16);
    }
    lab = make_label(outputs, "GREEN ON    GRAY OFF    ORANGE ALERT", FONT_S, MUTED);
    lv_obj_set_pos(lab, 3, 286);
}

/**
 * I/O：本地水浸/急停/继电器/蜂鸣器 + 远端 DI/DO
 * DO 开关绑定 Coil0..N；RS485/CAN 状态暂静态
 */
static void build_io(lv_obj_t *page)
{
    lv_obj_t *local = make_panel(page, 16, 10, 306, 292);
    lv_obj_t *remote;
    lv_obj_t *title;
    lv_obj_t *lab;
    uint8_t i;

    title = make_label(local, "LOCAL GPIO", FONT_M, GREEN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);
    add_line(local, 5, 34, 276);
    make_round_status(local, 78, 90, GREEN, LV_SYMBOL_OK, "WATER", "NORMAL",
                      &s_ui.io.water_icon, &s_ui.io.water_state);
    make_round_status(local, 210, 90, GREEN, LV_SYMBOL_OK, "E-STOP", "NORMAL",
                      NULL, NULL);
    add_line(local, 5, 171, 276);

    lab = make_label(local, "RELAY", FONT_S, WHITE);
    lv_obj_set_pos(lab, 20, 188);
    s_ui.io.relay_sw = make_switch(local, 174, 184, false);
    bind_coil_switch(s_ui.io.relay_sw, MODBUS_COIL_LED_PA3);
    s_ui.io.relay_label = make_live_label(local, "OFF", FONT_S, MUTED);
    lv_obj_set_pos(s_ui.io.relay_label, 238, 190);

    lab = make_label(local, "BUZZER", FONT_S, WHITE);
    lv_obj_set_pos(lab, 20, 238);
    s_ui.io.buzzer_sw = make_switch(local, 174, 234, false);
    bind_coil_switch(s_ui.io.buzzer_sw, 1U);
    s_ui.io.buzzer_label = make_live_label(local, "OFF", FONT_S, MUTED);
    lv_obj_set_pos(s_ui.io.buzzer_label, 238, 240);

    remote = make_panel(page, 330, 10, 454, 292);
    title = make_label(remote, "REMOTE DI / DO", FONT_M, GREEN);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 0);
    add_line(remote, 5, 34, 424);
    lab = make_label(remote, "DIGITAL INPUTS (8)", FONT_S, WHITE);
    lv_obj_set_pos(lab, 8, 45);
    for (i = 0; i < 8; i++) {
        char n[3];
        lv_snprintf(n, sizeof(n), "%d", i + 1);
        lab = make_label(remote, n, FONT_S, WHITE);
        lv_obj_set_pos(lab, 25 + i * 51, 75);
        make_led(remote, 20 + i * 51, 96, LIME, false, 22);
    }
    add_line(remote, 5, 133, 424);
    lab = make_label(remote, "DIGITAL OUTPUTS (8)", FONT_S, WHITE);
    lv_obj_set_pos(lab, 8, 145);
    for (i = 0; i < 8; i++) {
        char n[3];
        lv_obj_t *sw;
        lv_snprintf(n, sizeof(n), "%d", i + 1);
        lab = make_label(remote, n, FONT_S, WHITE);
        lv_obj_set_pos(lab, 25 + i * 51, 174);
        sw = lv_switch_create(remote);
        lv_obj_set_pos(sw, 14 + i * 51, 194);
        lv_obj_set_size(sw, 34, 50);
        lv_obj_set_style_bg_color(sw, color(0x31465D), LV_PART_MAIN);
        lv_obj_set_style_bg_color(sw, color(GREEN), LV_PART_INDICATOR | LV_STATE_CHECKED);
        lv_obj_set_style_bg_color(sw, color(WHITE), LV_PART_KNOB);
        if (i < MODBUS_COIL_COUNT) {
            bind_coil_switch(sw, (uint16_t)i);
        }
        lab = make_label(remote, "OFF", FONT_S, MUTED);
        lv_obj_set_pos(lab, 18 + i * 51, 252);
    }

    make_led(page, 32, 322, LIME, true, 12);
    lab = make_label(page, "RS485  NORMAL", FONT_S, WHITE);
    lv_obj_set_pos(lab, 52, 318);
    make_led(page, 194, 322, LIME, true, 12);
    lab = make_label(page, "CAN  NORMAL", FONT_S, WHITE);
    lv_obj_set_pos(lab, 214, 318);
}

/** Parameters 列表一行：图标 + 名称 + 副标题 + 值或开关 */
static void add_setting_row(lv_obj_t *parent, int y, const char *icon, const char *name,
                            const char *sub, const char *value, bool toggle)
{
    make_label(parent, icon, FONT_S, GREEN);
    lv_obj_set_pos(lv_obj_get_child(parent, (int32_t)lv_obj_get_child_count(parent) - 1), 12, y + 4);
    make_label(parent, name, FONT_S, WHITE);
    lv_obj_set_pos(lv_obj_get_child(parent, (int32_t)lv_obj_get_child_count(parent) - 1), 54, y + 1);
    if (sub != NULL) {
        make_label(parent, sub, FONT_S, MUTED);
        lv_obj_set_pos(lv_obj_get_child(parent, (int32_t)lv_obj_get_child_count(parent) - 1), 54, y + 17);
    }
    if (toggle) {
        make_switch(parent, 675, y + 3, true);
    } else {
        lv_obj_t *v = make_label(parent, value, FONT_S, WHITE);
        lv_obj_align(v, LV_ALIGN_TOP_RIGHT, -24, y + 5);
    }
    add_line(parent, 8, y + 33, 726);
}

/**
 * Parameters：阈值/告警/Modbus/IP 等（当前多为静态展示）
 * APPLY / RESTORE 按钮尚未挂业务回调
 */
static void build_parameters(lv_obj_t *page)
{
    lv_obj_t *list = make_panel(page, 14, 8, 772, 286);
    lv_obj_t *apply;
    lv_obj_t *restore;
    lv_obj_t *l;

    add_setting_row(list, 0, LV_SYMBOL_UP, "Temperature high limit", "Range -40.0 to 120.0 C", "60.0 C", false);
    add_setting_row(list, 34, LV_SYMBOL_DOWN, "Temperature low limit", "Range -40.0 to 120.0 C", "-10.0 C", false);
    add_setting_row(list, 68, LV_SYMBOL_UP, "Humidity high limit", "Range 0 to 100 %RH", "90 %RH", false);
    add_setting_row(list, 102, LV_SYMBOL_DOWN, "Humidity low limit", "Range 0 to 100 %RH", "10 %RH", false);
    add_setting_row(list, 136, LV_SYMBOL_BELL, "Alarm enabled", NULL, NULL, true);
    add_setting_row(list, 170, LV_SYMBOL_VOLUME_MAX, "Buzzer enabled", NULL, NULL, true);
    add_setting_row(list, 204, LV_SYMBOL_SETTINGS, "Modbus address", "Range 1 to 247", "001", false);
    add_setting_row(list, 238, LV_SYMBOL_WIFI, "IP address", "IPv4 network address", "192.168.1.100", false);

    apply = lv_button_create(page);
    lv_obj_set_pos(apply, 14, 304);
    lv_obj_set_size(apply, 374, 38);
    lv_obj_set_style_bg_color(apply, color(GREEN), 0);
    lv_obj_set_style_radius(apply, 5, 0);
    l = make_label(apply, "APPLY", FONT_S, 0x052016);
    lv_obj_center(l);

    restore = lv_button_create(page);
    lv_obj_set_pos(restore, 398, 304);
    lv_obj_set_size(restore, 388, 38);
    lv_obj_set_style_bg_color(restore, color(PANEL), 0);
    lv_obj_set_style_border_color(restore, color(MUTED), 0);
    lv_obj_set_style_border_width(restore, 1, 0);
    lv_obj_set_style_radius(restore, 5, 0);
    l = make_label(restore, "RESTORE", FONT_S, WHITE);
    lv_obj_center(l);
}

/** System 信息行：左图标/名称，右对齐取值 */
static void add_info_row(lv_obj_t *parent, int y, const char *icon, const char *name,
                         const char *value, uint32_t value_color)
{
    make_label(parent, icon, FONT_S, GREEN);
    lv_obj_set_pos(lv_obj_get_child(parent, (int32_t)lv_obj_get_child_count(parent) - 1), 10, y + 3);
    make_label(parent, name, FONT_S, WHITE);
    lv_obj_set_pos(lv_obj_get_child(parent, (int32_t)lv_obj_get_child_count(parent) - 1), 50, y + 4);
    {
        lv_obj_t *v = make_label(parent, value, FONT_S, value_color);
        lv_obj_align(v, LV_ALIGN_TOP_RIGHT, -12, y + 4);
    }
    add_line(parent, 6, y + 28, 724);
}

/**
 * System：固件/序列号/运行时间/双核与网络状态 + 维护区
 * 运行时间按 HAL_GetTick 在建页时计算；亮度条暂静态 80%
 */
static void build_system(lv_obj_t *page)
{
    lv_obj_t *info = make_panel(page, 20, 8, 760, 240);
    lv_obj_t *maint;
    lv_obj_t *bar;
    lv_obj_t *l;
    char uptime[32];
    uint32_t sec = HAL_GetTick() / 1000U;
    uint32_t days = sec / 86400U;
    uint32_t hh = (sec / 3600U) % 24U;
    uint32_t mm = (sec / 60U) % 60U;
    uint32_t ss = sec % 60U;

    lv_snprintf(uptime, sizeof(uptime), "%lu days %02lu:%02lu:%02lu",
                (unsigned long)days, (unsigned long)hh, (unsigned long)mm, (unsigned long)ss);

    make_label(info, "SYSTEM INFORMATION", FONT_S, GREEN);
    lv_obj_set_pos(lv_obj_get_child(info, 0), 8, 0);
    add_line(info, 6, 28, 724);
    add_info_row(info, 30, LV_SYMBOL_SETTINGS, "Firmware version", "V1.0.2-CM7", WHITE);
    add_info_row(info, 59, LV_SYMBOL_LIST, "Device serial", "CG-H747-20260524", WHITE);
    add_info_row(info, 88, LV_SYMBOL_LOOP, "Uptime", uptime, WHITE);
    add_info_row(info, 117, LV_SYMBOL_OK, "Dual-core status", "CM7 NORMAL     CM4 NORMAL", GREEN);
    add_info_row(info, 146, LV_SYMBOL_WIFI, "Ethernet", "CONNECTED", GREEN);
    add_info_row(info, 175, LV_SYMBOL_REFRESH, "Modbus", "SLAVE ONLINE", GREEN);
    add_info_row(info, 204, LV_SYMBOL_DOWNLOAD, "OTA", "IDLE  0%", GREEN);

    maint = make_panel(page, 20, 256, 760, 86);
    make_label(maint, "MAINTENANCE", FONT_S, GREEN);
    lv_obj_set_pos(lv_obj_get_child(maint, 0), 8, 0);
    add_line(maint, 6, 28, 724);
    make_label(maint, LV_SYMBOL_EYE_OPEN, FONT_S, GREEN);
    lv_obj_set_pos(lv_obj_get_child(maint, 2), 12, 33);
    make_label(maint, "Screen brightness", FONT_S, WHITE);
    lv_obj_set_pos(lv_obj_get_child(maint, 3), 50, 36);
    bar = lv_bar_create(maint);
    lv_obj_set_pos(bar, 470, 38);
    lv_obj_set_size(bar, 210, 10);
    lv_bar_set_value(bar, 80, LV_ANIM_OFF);
    lv_obj_set_style_bg_color(bar, color(0x26394D), LV_PART_MAIN);
    lv_obj_set_style_bg_color(bar, color(GREEN), LV_PART_INDICATOR);
    make_label(maint, "80%", FONT_S, GREEN);
    lv_obj_set_pos(lv_obj_get_child(maint, (int32_t)lv_obj_get_child_count(maint) - 1), 690, 34);
    make_label(maint, "Language: ENGLISH", FONT_S, WHITE);
    lv_obj_set_pos(lv_obj_get_child(maint, (int32_t)lv_obj_get_child_count(maint) - 1), 50, 57);
    make_label(maint, "Factory reset", FONT_S, WHITE);
    lv_obj_set_pos(lv_obj_get_child(maint, (int32_t)lv_obj_get_child_count(maint) - 1), 300, 57);
    l = make_label(maint, "EXECUTE", FONT_S, RED);
    lv_obj_set_pos(l, 665, 57);
}

/* ==========================================================================
 * 切页调度
 * ========================================================================== */

/** 销毁页面前清空 live 指针，防止 refresh 访问已删对象 */
static void clear_live_ptrs(void)
{
    memset(&s_ui.ov, 0, sizeof(s_ui.ov));
    memset(&s_ui.io, 0, sizeof(s_ui.io));
}

/** 销毁当前页内容并按 id 重建 */
static void build_page(page_id_t id)
{
    clear_live_ptrs();
    lv_obj_clean(s_ui.page);

    /* clean 后重新确保页根不透明，盖住内容区 */
    if (s_ui.page != NULL) {
        lv_obj_set_style_bg_color(s_ui.page, color(BG), 0);
        lv_obj_set_style_bg_opa(s_ui.page, LV_OPA_COVER, 0);
    }

    switch (id) {
    case PAGE_OVERVIEW:
        build_overview(s_ui.page);
        break;
    case PAGE_IO:
        build_io(s_ui.page);
        break;
    case PAGE_PARAMETERS:
        build_parameters(s_ui.page);
        break;
    case PAGE_SYSTEM:
        build_system(s_ui.page);
        break;
    default:
        break;
    }
}

/**
 * 切页前物理清掉内容区 LTDC 像素（PARTIAL 只刷脏区，否则上一页会透出来）
 */
static void clear_content_fb(void)
{
    lcd_fill(0, (uint16_t)HEADER_H, (uint16_t)(SCREEN_W - 1),
             (uint16_t)(HEADER_H + CONTENT_H - 1), 0x0000);
}

/** 底栏选中/未选中样式 */
static void style_nav(page_id_t active)
{
    uint8_t i;
    for (i = 0; i < PAGE_COUNT; i++) {
        lv_obj_t *btn = s_ui.nav_buttons[i];
        lv_obj_t *label = lv_obj_get_child(btn, 0);
        if ((page_id_t)i == active) {
            lv_obj_set_style_bg_color(btn, color(0x0B2E31), 0);
            lv_obj_set_style_border_color(btn, color(GREEN), 0);
            lv_obj_set_style_text_color(label, color(GREEN), 0);
        } else {
            lv_obj_set_style_bg_color(btn, color(0x071727), 0);
            lv_obj_set_style_border_color(btn, color(BORDER), 0);
            lv_obj_set_style_text_color(label, color(MUTED), 0);
        }
    }
}

/**
 * 切换到指定页：同页已建则只刷新底栏高亮
 * 切页时先清 FB，再建页，再整屏重绘
 */
static void show_page(page_id_t id)
{
    if (id >= PAGE_COUNT) {
        id = PAGE_OVERVIEW;
    }
    if (s_ui.page != NULL && s_ui.active == id && lv_obj_get_child_count(s_ui.page) > 0) {
        style_nav(id);
        return;
    }
    s_ui.active = id;
    clear_content_fb();
    build_page(id);
    style_nav(id);
    if (s_ui.page != NULL) {
        lv_obj_invalidate(s_ui.page);
    }
    if (s_ui.content != NULL) {
        lv_obj_invalidate(s_ui.content);
    }
    lv_obj_invalidate(lv_screen_active());
    lv_refr_now(NULL); /* 立刻刷完，避免先露出黑底/残影 */
    cabinet_guard_ui_refresh();
}

/**
 * @brief  底部导航按钮事件回调
 */
static void nav_event(lv_event_t *e)
{
    page_id_t index = (page_id_t)(uintptr_t)lv_event_get_user_data(e);
    if (lv_event_get_code(e) != LV_EVENT_CLICKED) {
        return;
    }
#if CABINET_UI_OVERVIEW_ONLY
    (void)index; /* 测试模式：忽略切页，只留 Overview */
    return;
#else
    show_page(index);
#endif
}

/** 底栏四键，每键宽 200 */
static void build_navigation(lv_obj_t *screen)
{
    lv_obj_t *nav = lv_obj_create(screen);
    const char *labels[] = {
        LV_SYMBOL_HOME "\nOVERVIEW",
        LV_SYMBOL_SETTINGS "\nI / O",
        LV_SYMBOL_EDIT "\nPARAMETERS",
        LV_SYMBOL_LIST "\nSYSTEM",
    };
    uint8_t i;

    lv_obj_remove_style_all(nav);
    lv_obj_set_pos(nav, 0, HEADER_H + CONTENT_H);
    lv_obj_set_size(nav, SCREEN_W, NAV_H);
    lv_obj_set_style_bg_color(nav, color(0x051424), 0);
    lv_obj_set_style_bg_opa(nav, LV_OPA_COVER, 0);

    for (i = 0; i < PAGE_COUNT; i++) {
        lv_obj_t *button = lv_button_create(nav);
        lv_obj_t *l;
        s_ui.nav_buttons[i] = button;
        lv_obj_set_pos(button, i * 200, 0);
        lv_obj_set_size(button, 200, NAV_H);
        lv_obj_set_style_radius(button, 0, 0);
        lv_obj_set_style_shadow_width(button, 0, 0);
        lv_obj_set_style_border_width(button, 1, 0);
        l = make_label(button, labels[i], FONT_S, MUTED);
        lv_obj_set_style_text_align(l, LV_TEXT_ALIGN_CENTER, 0);
        lv_obj_center(l);
        lv_obj_add_event_cb(button, nav_event, LV_EVENT_CLICKED, (void *)(uintptr_t)i);
    }
}

/* ==========================================================================
 * 对外接口：刷新 / 创建
 * ========================================================================== */

/**
 * @brief 周期刷新（约 500ms，由 lvgl_demo 定时器调用）
 * - 顶栏时钟、WATER 灯
 * - Overview / I·O：温湿度、水浸告警、继电器/蜂鸣器
 * - Parameters / System：当前无动态字段
 */
void cabinet_guard_ui_refresh(void)
{
    uint16_t temp_c = BSP_AppData_GetTemperature();
    uint16_t humi = BSP_AppData_GetHumidity();
    uint8_t relay = BSP_Modbus_GetCoil(MODBUS_COIL_LED_PA3);
    uint8_t buzzer = BSP_Modbus_GetCoil(1U);
    uint32_t sec = HAL_GetTick() / 1000U;
    char buf[16];
    bool temp_ok;
    bool hum_ok;

    s_ui.wet_latched = BSP_AppData_GetWet() != 0U;

    if (s_ui.clock != NULL) {
        lv_snprintf(buf, sizeof(buf), "%02lu:%02lu",
                    (unsigned long)((sec / 3600U) % 24U),
                    (unsigned long)((sec / 60U) % 60U));
        lv_label_set_text(s_ui.clock, buf);
    }

    /* ETH/MOD/OTA/E-STOP 暂固定绿；WATER 跟水浸滞回 */
    set_led_state(s_ui.hdr_leds[0], GREEN, true);
    set_led_state(s_ui.hdr_leds[1], GREEN, true);
    set_led_state(s_ui.hdr_leds[2], GREEN, true);
    set_led_state(s_ui.hdr_leds[3], GREEN, true);
    set_led_state(s_ui.hdr_leds[4], s_ui.wet_latched ? RED : GREEN, true);

    if (s_ui.active == PAGE_OVERVIEW) {
        temp_ok = (temp_c <= 60U);
        hum_ok = (humi >= 10U && humi <= 90U);

        if (s_ui.ov.temp_value != NULL) {
            lv_snprintf(buf, sizeof(buf), "%u", (unsigned)temp_c);
            lv_label_set_text(s_ui.ov.temp_value, buf);
            lv_obj_set_style_text_color(s_ui.ov.temp_value, color(temp_ok ? GREEN : ORANGE), 0);
        }
        if (s_ui.ov.hum_value != NULL) {
            lv_snprintf(buf, sizeof(buf), "%u", (unsigned)humi);
            lv_label_set_text(s_ui.ov.hum_value, buf);
            lv_obj_set_style_text_color(s_ui.ov.hum_value, color(hum_ok ? GREEN : ORANGE), 0);
        }
        if (s_ui.ov.temp_status != NULL) {
            lv_label_set_text(s_ui.ov.temp_status, temp_ok ? "NORMAL" : "WARN");
            lv_obj_set_style_text_color(s_ui.ov.temp_status, color(temp_ok ? GREEN : ORANGE), 0);
        }
        if (s_ui.ov.hum_status != NULL) {
            lv_label_set_text(s_ui.ov.hum_status, hum_ok ? "NORMAL" : "WARN");
            lv_obj_set_style_text_color(s_ui.ov.hum_status, color(hum_ok ? GREEN : ORANGE), 0);
        }

        set_gauge(s_ui.ov.temp_arc, (int)temp_c > 80 ? 80 : (int)temp_c);
        set_gauge(s_ui.ov.hum_arc, (int)humi > 100 ? 100 : (int)humi);

        set_round_status(s_ui.ov.estop_icon, s_ui.ov.estop_state, GREEN, LV_SYMBOL_OK, "NORMAL");
        if (s_ui.wet_latched) {
            set_round_status(s_ui.ov.leak_icon, s_ui.ov.leak_state, RED, LV_SYMBOL_WARNING, "ALARM");
        } else {
            set_round_status(s_ui.ov.leak_icon, s_ui.ov.leak_state, GREEN, LV_SYMBOL_OK, "NORMAL");
        }

        sync_switch_label(s_ui.ov.relay_sw, s_ui.ov.relay_label, relay != 0U);
        sync_switch_label(s_ui.ov.buzzer_sw, s_ui.ov.buzzer_label, buzzer != 0U);
    } else if (s_ui.active == PAGE_IO) {
        if (s_ui.wet_latched) {
            set_round_status(s_ui.io.water_icon, s_ui.io.water_state, RED, LV_SYMBOL_WARNING, "ALARM");
        } else {
            set_round_status(s_ui.io.water_icon, s_ui.io.water_state, GREEN, LV_SYMBOL_OK, "NORMAL");
        }
        sync_switch_label(s_ui.io.relay_sw, s_ui.io.relay_label, relay != 0U);
        sync_switch_label(s_ui.io.buzzer_sw, s_ui.io.buzzer_label, buzzer != 0U);
    }
}

/**
 * @brief 创建整套 HMI（lvgl 任务启动时调用一次）
 * 1) lcd_clear 清 LTDC FB（防软复位残留旧 UI）
 * 2) 在默认 screen 上建 Header / Content / Nav（不另开 screen）
 * 3) 加载 Overview 首页
 */
void cabinet_guard_ui_create(void)
{
    lv_obj_t *screen;

    memset(&s_ui, 0, sizeof(s_ui));

    /* 物理清屏：去掉软复位后 SDRAM FB 里旧 Machine Dashboard 的像素 */
    lcd_clear(0x0000);

    /* 直接建在默认屏上，避免再留一层未删除的旧 screen */
    screen = lv_screen_active();
    lv_obj_clean(screen);
    lv_obj_remove_style_all(screen);
    lv_obj_set_style_bg_color(screen, color(BG), 0);
    lv_obj_set_style_bg_opa(screen, LV_OPA_COVER, 0);

    build_header(screen);

    /* 中间内容区：不透明底 + 不可滚动 */
    s_ui.content = lv_obj_create(screen);
    lv_obj_remove_style_all(s_ui.content);
    lv_obj_set_pos(s_ui.content, 0, HEADER_H);
    lv_obj_set_size(s_ui.content, SCREEN_W, CONTENT_H);
    lv_obj_set_style_bg_color(s_ui.content, color(BG), 0);
    lv_obj_set_style_bg_opa(s_ui.content, LV_OPA_COVER, 0);
    lv_obj_remove_flag(s_ui.content, LV_OBJ_FLAG_SCROLLABLE);

    /* 当前页根：切页时 lv_obj_clean(this) */
    s_ui.page = lv_obj_create(s_ui.content);
    lv_obj_remove_style_all(s_ui.page);
    lv_obj_set_size(s_ui.page, SCREEN_W, CONTENT_H);
    lv_obj_set_pos(s_ui.page, 0, 0);
    lv_obj_set_style_bg_color(s_ui.page, color(BG), 0);
    lv_obj_set_style_bg_opa(s_ui.page, LV_OPA_COVER, 0);
    lv_obj_remove_flag(s_ui.page, LV_OBJ_FLAG_SCROLLABLE);

    build_navigation(screen);

    s_ui.active = PAGE_COUNT; /* 强制首次 build_overview */
    show_page(PAGE_OVERVIEW);
#if CABINET_UI_OVERVIEW_ONLY
    /* 测试：底栏仅高亮 Overview，其它键无响应 */
    style_nav(PAGE_OVERVIEW);
#endif
    lv_obj_invalidate(screen);
}
