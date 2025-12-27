//
// Created by 19571 on 2025/12/28.
//

#include "ui_message_handler.h"

#include <stdio.h>
#include <string.h>

#include "build/Release/_deps/lvgl-src/src/misc/lv_types.h"
#include "components/ui_comp_comp_deviceitem.h"

extern lv_obj_t * ui_Panel1;


void on_connect_click_event(lv_event_t * e)
{
    lv_obj_t * btn = lv_event_get_target(e);
    char * mac_addr = (char *)lv_event_get_user_data(e);

    printf("Clicked Connect on device: %s\n", mac_addr);

    // 这里可以发送消息回 业务线程/蓝牙线程 去发起连接
}

void UI_AddBleItem(const char * name, const char * mac)
{
    // 1. 创建组件实例
    // 注意：ui_Comp_DeviceItem_create 返回的是组件的最外层容器
    lv_obj_t * new_item = ui_Comp_DeviceItem_create(ui_Panel1);

    // 2. 获取内部子对象
    // SquareLine 提供了 ui_comp_get_child 函数，通过索引获取组件内部的具体对象
    lv_obj_t * lbl_name = ui_comp_get_child(new_item, UI_COMP_COMP_DEVICEITEM_PANEL7_LABEL3);
    lv_obj_t * lbl_mac  = ui_comp_get_child(new_item, UI_COMP_COMP_DEVICEITEM_PANEL7_LABEL4);

    // 3. 修改文本
    if (lbl_name) {
        lv_label_set_text(lbl_name, name);
    }

    if (lbl_mac) {
        lv_label_set_text(lbl_mac, mac);
    }

    // 4. (可选) 给这个新生成的 Item 里的“连接”按钮绑定事件
    // 获取按钮对象
    lv_obj_t * btn_connect = ui_comp_get_child(new_item, UI_COMP_COMP_DEVICEITEM_BUTTON5);

    if (btn_connect) {
        // 我们可以把 MAC 地址作为 user_data 传进去，这样点击时就知道连哪个设备了
        // 注意：这里需要动态申请内存保存 MAC，否则函数退出后 mac 指针失效
        // 简单演示使用 strdup (需要 #include <string.h> 和 stdlib)
        char * mac_data = strdup(mac);
        lv_obj_add_event_cb(btn_connect, on_connect_click_event, LV_EVENT_CLICKED, mac_data);
    }
}