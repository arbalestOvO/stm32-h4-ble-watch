//
// Created by 19571 on 2025/12/28.
//

#ifndef ABOLUO_EXIT_UI_PROTOCOL_H
#define ABOLUO_EXIT_UI_PROTOCOL_H
#include <stdint.h>

#include "tx_api.h"

// --- 1. 定义事件类型 ---
typedef enum {
    UI_EVENT_BLE_FOUND,     // 蓝牙扫描结果
} ui_event_type_t;

// --- 2. 定义各种业务的负载数据结构 ---

// 蓝牙负载
typedef struct {
    char mac[18];
    char name[32];
} payload_ble_t;

// --- 3. 定义统一的消息包 (核心) ---
typedef struct {
    ui_event_type_t type;  // 消息类型，决定了怎么读 payload

    // 使用 union 共享内存空间，大小等于最大的那个结构体
    union {
        payload_ble_t   ble;
    } payload;

} ui_message_t;

// 全局 UI 队列
#define UI_QUEUE_SIZE 20


#endif //ABOLUO_EXIT_UI_PROTOCOL_H