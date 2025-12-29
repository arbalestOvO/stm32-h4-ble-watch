//
// Created by 19571 on 2025/12/28.
//

#include "ui_message_handler.h"

#include <stdio.h>
#include <string.h>

#include "tx_api.h"
#include "ui_interface.h"
#include "build/Release/_deps/lvgl-src/src/misc/lv_types.h"
#include "components/ui_comp_comp_deviceitem.h"

extern lv_obj_t * ui_Panel1;
extern TX_QUEUE app_client_queue;

void on_connect_click_event(lv_event_t * e)
{
    lv_obj_t * btn = lv_event_get_target(e);
    char * mac_addr = (char *)lv_event_get_user_data(e);

    printf("Clicked Connect on device: %s\n", mac_addr);
    ui_show_notify_safe(true, false, "正在连接中");
    // 这里可以发送消息回 业务线程/蓝牙线程 去发起连接
    tx_queue_send(&app_client_queue, mac_addr, TX_NO_WAIT);
}

lv_obj_t * Find_Item_By_Mac(lv_obj_t * parent_panel, const char * target_mac)
{
    uint32_t child_cnt = lv_obj_get_child_count(parent_panel);

    for(uint32_t i = 0; i < child_cnt; i++) {
        lv_obj_t * item = lv_obj_get_child(parent_panel, i);

        // 我们利用之前存入 Button 的 user_data (保存了MAC) 来识别这个 Item
        // 或者，我们需要通过 UI_COMP_... 索引去获取显示 MAC 的 Label 内容来对比

        // 方案 B：获取 Label 内容对比 (比较慢但不需要额外内存)
        // 假设 ui_Comp_DeviceItem 结构没变，我们去拿 MAC Label
        lv_obj_t * lbl_mac = ui_comp_get_child(item, UI_COMP_COMP_DEVICEITEM_PANEL7_LABEL4);
        if (lbl_mac) {
            const char * txt = lv_label_get_text(lbl_mac);
            if (strcmp(txt, target_mac) == 0) {
                return item; // 找到了！
            }
        }
    }
    return NULL; // 没找到
}

// 修改后的添加函数
void UI_AddOrUpdateBleItem(const char * name, const char * mac)
{
    // 1. 先检查是否存在
    lv_obj_t * existing_item = Find_Item_By_Mac(ui_Panel1, mac);

    if (existing_item != NULL) {
        // --- 情况 A: 设备已存在 ---
        // 可以在这里更新 RSSI 或 名字 (如果名字变了)
        // printf("Device %s exists, skipping...\n", mac);
        lv_obj_t * lbl_name = ui_comp_get_child(existing_item, UI_COMP_COMP_DEVICEITEM_PANEL7_LABEL3);
        if(lbl_name && strcmp("(Unknown)", name) != 0) lv_label_set_text(lbl_name, name);
        return;
    }

    // --- 情况 B: 新设备，且内存足够 ---

    // 2. 在创建前，最好检查一下内存 (可选，但推荐)
    lv_mem_monitor_t mon;
    lv_mem_monitor(&mon);
    if (mon.free_size < 2048) { // 假设一个 Item 需要 1-2KB
        printf("LVGL Memory Low! Cannot add new device.\n");
        return;
    }

    // 3. 安全创建
    // SquareLine 生成的代码内部没有判空保护，所以如果这里崩溃，
    // 唯一的办法就是上面步骤1提到的：增大 LV_MEM_SIZE
    lv_obj_t * new_item = ui_Comp_DeviceItem_create(ui_Panel1);

    // ... 设置 Label 和 事件绑定代码 (同之前) ...
    lv_obj_t * lbl_name = ui_comp_get_child(new_item, UI_COMP_COMP_DEVICEITEM_PANEL7_LABEL3);
    lv_obj_t * lbl_mac  = ui_comp_get_child(new_item, UI_COMP_COMP_DEVICEITEM_PANEL7_LABEL4);
    if(lbl_name) lv_label_set_text(lbl_name, name);
    if(lbl_mac) lv_label_set_text(lbl_mac, mac);
    lv_obj_t * btn_connect = ui_comp_get_child(new_item, UI_COMP_COMP_DEVICEITEM_BUTTON5);

    if (btn_connect) {
        // 我们可以把 MAC 地址作为 user_data 传进去，这样点击时就知道连哪个设备了
        // 注意：这里需要动态申请内存保存 MAC，否则函数退出后 mac 指针失效
        // 简单演示使用 strdup (需要 #include <string.h> 和 stdlib)
        char * mac_data = strdup(mac);
        lv_obj_add_event_cb(btn_connect, on_connect_click_event, LV_EVENT_CLICKED, mac_data);
    }
}

void UI_AddBleItem(const char * name, const char * mac)
{
    if (strcmp("(Unknown)", name) == 0) return;
    UI_AddOrUpdateBleItem(name, mac);
}