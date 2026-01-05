#include "watch_menu.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "auth_client.h"
#include "hichain_utils.h"
#include "huawei_tlv.h"
#include "lvgl.h"
#include "TimeUtils.h"
#include "ui.h"

// ---------------------------------------------------------
// 全局变量与对象句柄
// ---------------------------------------------------------
static lv_obj_t * g_menu_screen = NULL;
static lv_obj_t * g_kb = NULL;          // 全局键盘句柄
static lv_obj_t * g_ta_hour = NULL;     // 小时输入框
static lv_obj_t * g_ta_min = NULL;      // 分钟输入框
static lv_obj_t * g_alert_modal = NULL; // 被查找时的弹窗

// ---------------------------------------------------------
// 内部回调函数声明
// ---------------------------------------------------------
static void ta_event_cb(lv_event_t * e);
static void btn_set_time_cb(lv_event_t * e);
static void btn_find_phone_cb(lv_event_t * e);
static void alert_close_btn_cb(lv_event_t * e);

// ---------------------------------------------------------
// 公开接口实现
// ---------------------------------------------------------

void trigger_find_my_watch(void) {
    if (g_alert_modal == NULL) return;

    // 显示弹窗
    lv_obj_remove_flag(g_alert_modal, LV_OBJ_FLAG_HIDDEN); // v9 API 推荐使用 remove_flag 替代 clear_flag
    
    // 确保弹窗在最上层
    lv_obj_move_foreground(g_alert_modal);

    // TODO: 在这里调用底层硬件接口，开始 震动 或 播放铃声
    // HAL_Buzzer_Start();
    // HAL_Motor_Start();
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_SET);
    printf("DEBUG: Ringing started! Waiting for user or phone to stop.\n");
}

void stop_find_my_watch(void) {
    if (g_alert_modal == NULL) return;

    // 隐藏弹窗
    lv_obj_add_flag(g_alert_modal, LV_OBJ_FLAG_HIDDEN);

    // TODO: 在这里调用底层硬件接口，停止 震动 或 铃声
    HAL_GPIO_WritePin(GPIOB, GPIO_PIN_1, GPIO_PIN_RESET);
    // HAL_Buzzer_Stop();
    // HAL_Motor_Stop();
    printf("DEBUG: Ringing stopped.\n");
}

// ---------------------------------------------------------
// UI 构建逻辑
// ---------------------------------------------------------

// 构建“被查找”的模态弹窗
static void create_alert_modal(lv_obj_t * parent) {
    // 创建一个居中容器作为弹窗
    g_alert_modal = lv_obj_create(parent);
    lv_obj_set_size(g_alert_modal, 240, 200); // 根据实际屏幕尺寸调整
    lv_obj_center(g_alert_modal);
    lv_obj_set_flex_flow(g_alert_modal, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(g_alert_modal, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    
    // 样式：红色边框示警
    lv_obj_set_style_border_color(g_alert_modal, lv_palette_main(LV_PALETTE_RED), 0);
    lv_obj_set_style_border_width(g_alert_modal, 3, 0);

    // 提示文本
    lv_obj_t * label = lv_label_create(g_alert_modal);
    lv_label_set_text(label, "查找设备\n响铃中...");
    lv_obj_set_style_text_font(label, &ui_font_Font1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);

    // 关闭按钮 (手表端手动关闭)
    // LVGL v9: lv_btn_create -> lv_button_create
    lv_obj_t * btn_stop = lv_button_create(g_alert_modal);
    lv_obj_set_width(btn_stop, 100);
    lv_obj_add_event_cb(btn_stop, alert_close_btn_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * btn_lbl = lv_label_create(btn_stop);
    lv_label_set_text(btn_lbl, "停止");
    lv_obj_set_style_text_font(btn_lbl, &ui_font_Font1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_center(btn_lbl);

    // 默认隐藏，等待 trigger_find_my_watch 调用
    lv_obj_add_flag(g_alert_modal, LV_OBJ_FLAG_HIDDEN);
}

void switchToMenu(void) {
    // 如果屏幕已经存在，直接加载
    if (g_menu_screen) {
        // LVGL v9: lv_scr_load -> lv_screen_load
        lv_screen_load(g_menu_screen);
        return;
    }

    g_menu_screen = lv_obj_create(NULL);
    // 设置主布局：垂直排列
    lv_obj_set_flex_flow(g_menu_screen, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(g_menu_screen, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_all(g_menu_screen, 10, 0);
    lv_obj_set_style_pad_row(g_menu_screen, 15, 0); // 控件垂直间距

    // --------------------------------------
    // 1. 时间设置区域
    // --------------------------------------
    lv_obj_t * time_cont = lv_obj_create(g_menu_screen);
    lv_obj_set_size(time_cont, lv_pct(100), LV_SIZE_CONTENT);
    lv_obj_set_flex_flow(time_cont, LV_FLEX_FLOW_ROW);
    lv_obj_set_flex_align(time_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_border_width(time_cont, 0, 0); // 去除容器边框

    // 小时输入框
    g_ta_hour = lv_textarea_create(time_cont);
    lv_textarea_set_placeholder_text(g_ta_hour, "HH");
    lv_textarea_set_max_length(g_ta_hour, 2);
    lv_textarea_set_one_line(g_ta_hour, true);
    lv_obj_set_width(g_ta_hour, 60);
    lv_obj_add_event_cb(g_ta_hour, ta_event_cb, LV_EVENT_ALL, NULL);

    // 冒号分隔符
    lv_obj_t * colon = lv_label_create(time_cont);
    lv_label_set_text(colon, ":");

    // 分钟输入框
    g_ta_min = lv_textarea_create(time_cont);
    lv_textarea_set_placeholder_text(g_ta_min, "MM");
    lv_textarea_set_max_length(g_ta_min, 2);
    lv_textarea_set_one_line(g_ta_min, true);
    lv_obj_set_width(g_ta_min, 60);
    lv_obj_add_event_cb(g_ta_min, ta_event_cb, LV_EVENT_ALL, NULL);

    // 确定按钮
    // LVGL v9: lv_btn_create -> lv_button_create
    lv_obj_t * btn_set = lv_button_create(g_menu_screen);
    lv_obj_set_width(btn_set, lv_pct(80));
    lv_obj_add_event_cb(btn_set, btn_set_time_cb, LV_EVENT_CLICKED, NULL);
    lv_obj_t * lbl_set = lv_label_create(btn_set);
    lv_label_set_text(lbl_set, "设置时间");
    lv_obj_set_style_text_font(lbl_set, &ui_font_Font1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_center(lbl_set);

    // --------------------------------------
    // 2. 查找手机按钮
    // --------------------------------------
    // LVGL v9: lv_btn_create -> lv_button_create
    lv_obj_t * btn_find = lv_button_create(g_menu_screen);
    lv_obj_set_width(btn_find, lv_pct(80));
    lv_obj_set_style_bg_color(btn_find, lv_palette_main(LV_PALETTE_ORANGE), 0);
    lv_obj_add_event_cb(btn_find, btn_find_phone_cb, LV_EVENT_CLICKED, NULL);
    
    lv_obj_t * lbl_find = lv_label_create(btn_find);
    lv_label_set_text(lbl_find, "查找设备");
    lv_obj_set_style_text_font(lbl_find, &ui_font_Font1, LV_PART_MAIN | LV_STATE_DEFAULT);
    lv_obj_center(lbl_find);

    // --------------------------------------
    // 3. 数字键盘 (默认隐藏，点击输入框弹出)
    // --------------------------------------
    g_kb = lv_keyboard_create(g_menu_screen);
    lv_keyboard_set_mode(g_kb, LV_KEYBOARD_MODE_NUMBER);
    lv_obj_add_flag(g_kb, LV_OBJ_FLAG_HIDDEN);

    // --------------------------------------
    // 4. 初始化“被查找”弹窗
    // --------------------------------------
    create_alert_modal(g_menu_screen);

    // 加载屏幕
    // LVGL v9: lv_scr_load -> lv_screen_load
    lv_screen_load(g_menu_screen);
}

// ---------------------------------------------------------
// 事件回调具体实现
// ---------------------------------------------------------

static void ta_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * ta = lv_event_get_target(e);

    if (code == LV_EVENT_CLICKED || code == LV_EVENT_FOCUSED) {
        if (g_kb != NULL) {
            lv_keyboard_set_textarea(g_kb, ta);
            lv_obj_remove_flag(g_kb, LV_OBJ_FLAG_HIDDEN); // v9 API remove_flag
            lv_obj_move_foreground(g_kb);
        }
    }
    else if (code == LV_EVENT_DEFOCUSED) {
        if (g_kb != NULL) {
             lv_keyboard_set_textarea(g_kb, NULL);
             lv_obj_add_flag(g_kb, LV_OBJ_FLAG_HIDDEN);
        }
    }
}

static void ble_send_time_sync(int h, int m) {
    SystemTime_t time;
    time.year = 25;
    time.month = 12;
    time.date = 20;
    time.hours = h;
    time.minutes = m;
    time.seconds = 0;
    Time_SetCalendar(&time);
    int time_v = Time_GetUnixTimestamp() / 1000 - 28800;
    uint8_t* data_0105 = (uint8_t*)malloc(12);
    htlv_writer_t writer;
    htlv_writer_init(&writer, data_0105, 12);
    data_0105[0] = 0x01,data_0105[1] = 0x05;
    writer.offset += 2;
    htlv_write_int(&writer, 0x01, time_v);
    htlv_write_short(&writer, 0x02, 0x0800);
    size_t data_len = writer.offset;
    send_app_tlv(writer.buffer, data_len, true);
}

static void btn_set_time_cb(lv_event_t * e) {
    const char * hour_str = lv_textarea_get_text(g_ta_hour);
    const char * min_str = lv_textarea_get_text(g_ta_min);

    // 简单的校验
    if (strlen(hour_str) == 0 || strlen(min_str) == 0) {
        printf("DEBUG: Time is empty\n");
        return;
    }

    int h = atoi(hour_str);
    int m = atoi(min_str);

    printf("DEBUG: User set time to %02d:%02d\n", h, m);

    // TODO: 将 h 和 m 封装成协议包，通过蓝牙(BLE)或网络发送给手机
    ble_send_time_sync(h, m);

    // 收起键盘
    lv_obj_add_flag(g_kb, LV_OBJ_FLAG_HIDDEN);
}

static void protocol_send_find_phone_cmd(void) {
    size_t len;
    uint8_t* data = hex_string_to_bytes("3701011968772e756e6974656465766963652e66696e64646576696365020a66696e64446576696365830b8409050435a66fa2060102", &len);
    send_app_tlv(data, len, true);
}

static void btn_find_phone_cb(lv_event_t * e) {
    printf("DEBUG: Find Phone button clicked!\n");

    // TODO: 调用查找手机功能接口
    // 该接口应向手机发送指令，使手机响铃
    protocol_send_find_phone_cmd();
}

static void alert_close_btn_cb(lv_event_t * e) {
    stop_find_my_watch();
}