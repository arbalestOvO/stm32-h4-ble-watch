#include "ui_interface.h"
#include "lvgl.h"
#include <stdbool.h>

#include "tx_api.h"
#include "ui.h"

extern TX_MUTEX lvgl_mutex;

// --- 全局句柄管理 ---
typedef struct {
    lv_obj_t * root;      // 根容器（背景遮罩或悬浮框）
    lv_obj_t * panel;     // 内容面板（用于布局）
    lv_obj_t * label;     // 文本
    lv_obj_t * bar;       // 进度条 (可选)
    lv_obj_t * spinner;   // 转圈圈 (可选)
} notify_obj_t;

static notify_obj_t g_notify = {0};

/**
 * @brief 显示通知/进度框
 * @param is_blocking  true: 模态（全屏遮挡，禁止点击底部）；false: 非模态（悬浮，允许点击底部）
 * @param show_bar     true: 显示进度条；false: 显示无限旋转的 Spinner
 * @param text         提示文本
 */
void ui_show_notify(bool is_blocking, bool show_bar, const char * text) {
    // 1. 清理旧通知
    if (g_notify.root != NULL) {
        lv_obj_delete(g_notify.root);
        memset(&g_notify, 0, sizeof(notify_obj_t));
    }

    // 2. 创建根对象 (挂载在 layer_top 保证最顶层)
    g_notify.root = lv_obj_create(lv_layer_top());
    lv_obj_remove_flag(g_notify.root, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_style_border_width(g_notify.root, 0, 0);

    // ==========================================
    // 分支 A: 阻塞模式 (全屏, 居中, 复杂内容)
    // ==========================================
    if (is_blocking) {
        // 全屏半透明黑色遮罩
        lv_obj_set_size(g_notify.root, LV_PCT(100), LV_PCT(100));
        lv_obj_set_style_bg_color(g_notify.root, lv_color_black(), 0);
        lv_obj_set_style_bg_opa(g_notify.root, LV_OPA_50, 0);
        lv_obj_add_flag(g_notify.root, LV_OBJ_FLAG_CLICKABLE); // 拦截点击

        // 创建居中面板
        g_notify.panel = lv_obj_create(g_notify.root);
        lv_obj_center(g_notify.panel);
        lv_obj_set_size(g_notify.panel, 220, LV_SIZE_CONTENT);
        lv_obj_set_flex_flow(g_notify.panel, LV_FLEX_FLOW_COLUMN);
        lv_obj_set_flex_align(g_notify.panel, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
        lv_obj_set_style_pad_all(g_notify.panel, 20, 0);
        lv_obj_set_style_pad_row(g_notify.panel, 15, 0);

        // 根据 show_bar 决定显示 进度条 还是 Spinner
        if (show_bar) {
            g_notify.bar = lv_bar_create(g_notify.panel);
            lv_obj_set_size(g_notify.bar, 180, 15);
            lv_bar_set_range(g_notify.bar, 0, 100);
            lv_bar_set_value(g_notify.bar, 0, LV_ANIM_OFF);
        } else {
            g_notify.spinner = lv_spinner_create(g_notify.panel);
            lv_spinner_set_anim_params(g_notify.spinner, 1000, 60);
            lv_obj_set_size(g_notify.spinner, 40, 40);
        }

        // 文本 (居中对齐)
        g_notify.label = lv_label_create(g_notify.panel);
        lv_label_set_text(g_notify.label, text);
        lv_obj_set_width(g_notify.label, LV_PCT(100));
        lv_label_set_long_mode(g_notify.label, LV_LABEL_LONG_WRAP);
        lv_obj_set_style_text_align(g_notify.label, LV_TEXT_ALIGN_CENTER, 0);
    }
    // ==========================================
    // 分支 B: 非阻塞模式 (左上角, 纯文本)
    // ==========================================
    else {
        // 设置根容器为自适应大小，且不拦截全屏点击
        lv_obj_set_size(g_notify.root, LV_SIZE_CONTENT, LV_SIZE_CONTENT);
        // 定位到左上角 (padding 10px)
        lv_obj_align(g_notify.root, LV_ALIGN_TOP_LEFT, 10, 10);

        // 样式优化：类似 HUD 的半透明黑底白字，或者亮色底
        // 这里采用：深色背景，白色文字，看起来像系统状态栏消息
        lv_obj_set_style_bg_color(g_notify.root, lv_palette_main(LV_PALETTE_GREY), 0); // 或者 lv_color_black()
        lv_obj_set_style_bg_opa(g_notify.root, LV_OPA_90, 0);
        lv_obj_set_style_radius(g_notify.root, 4, 0); //稍微圆角
        lv_obj_set_style_pad_all(g_notify.root, 8, 0); // 内边距小一点

        // 关键：根容器本身不拦截点击 (remove CLICKABLE)，允许穿透
        // 如果你希望文字背景区域能点(比如点一下关闭)，则 add flag
        // 这里默认穿透：
        lv_obj_remove_flag(g_notify.root, LV_OBJ_FLAG_CLICKABLE);

        // 纯文本内容
        g_notify.panel = g_notify.root; // 面板就是根
        g_notify.label = lv_label_create(g_notify.panel);
        lv_label_set_text(g_notify.label, text);
        lv_obj_set_style_text_color(g_notify.label, lv_color_white(), 0); // 白字

        // 强制置空其他对象，确保安全
        g_notify.bar = NULL;
        g_notify.spinner = NULL;
    }
    if (g_notify.label != NULL) {
        lv_obj_set_style_text_font(g_notify.label, &ui_font_Font1, LV_PART_MAIN | LV_STATE_DEFAULT);
    }
}

/**
 * @brief 更新进度 (仅在 show_bar=true 时有效)
 */
void ui_update_notify_progress(int32_t value) {
    if (g_notify.bar != NULL) {
        lv_bar_set_value(g_notify.bar, value, LV_ANIM_ON);
    }
}

/**
 * @brief 更新文字提示
 */
void ui_update_notify_text(const char * text) {
    if (g_notify.label != NULL) {
        lv_label_set_text(g_notify.label, text);
    }
}

/**
 * @brief 关闭通知
 */
void ui_close_notify(void) {
    if (g_notify.root != NULL) {
        lv_obj_delete(g_notify.root);
        // 清空结构体，防止悬空指针
        memset(&g_notify, 0, sizeof(notify_obj_t));
    }
}

extern TX_THREAD * g_gui_thread_ptr;

/**
 * @brief 安全锁开始宏
 * * 逻辑：
 * 1. 创建一个局部作用域 do { ...
 * 2. 获取当前线程 ID
 * 3. 判断是否需要加锁（如果当前不是 GUI 线程，且 GUI 线程已启动，则加锁）
 * 4. 标记锁状态，供 END 宏使用
 */
#define LV_GUI_SAFE_BEGIN \
do { \
TX_THREAD * _curr_thread_ = tx_thread_identify(); \
bool _need_lock_ = (_curr_thread_ != g_gui_thread_ptr && g_gui_thread_ptr != NULL); \
if (_need_lock_) { \
tx_mutex_get(&lvgl_mutex, TX_WAIT_FOREVER); \
}

/**
 * @brief 安全锁结束宏
 * * 逻辑：
 * 1. 检查 BEGIN 宏中定义的 _need_lock_ 状态
 * 2. 如果之前加了锁，这里就解锁
 * 3. 结束局部作用域 ... } while(0)
 */
#define LV_GUI_SAFE_END \
if (_need_lock_) { \
tx_mutex_put(&lvgl_mutex); \
} \
} while(0);

void ui_show_notify_safe(bool is_blocking, bool show_bar, const char* text) {
    LV_GUI_SAFE_BEGIN
    ui_show_notify(is_blocking, show_bar, text);
    LV_GUI_SAFE_END
}

void app_update_progress_safe(int percent) {
    LV_GUI_SAFE_BEGIN
    ui_update_notify_progress(percent);
    LV_GUI_SAFE_END
}

void app_close_notify_safe(void) {
    LV_GUI_SAFE_BEGIN
    ui_close_notify();
    LV_GUI_SAFE_END
}