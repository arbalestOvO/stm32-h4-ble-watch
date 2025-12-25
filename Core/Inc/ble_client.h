//
// Created by 19571 on 2025/12/24.
//

#ifndef ABOLUO_EXIT_BLE_CLIENT_H
#define ABOLUO_EXIT_BLE_CLIENT_H

#include "tx_api.h"
#include "stm32h7xx_hal.h"
#include <string.h>
#include <stdio.h>

/* ----------------配置参数---------------- */
#define BLE_RX_BUF_SIZE         512     // 行缓冲区大小
#define BLE_DEFAULT_TIMEOUT     2000    // 默认超时(ms)
#define BLE_CONNECT_TIMEOUT     10000   // 连接超时(ms)
#define BLE_CONN_ID             0       // 默认连接ID

/* ----------------外部依赖---------------- */
extern TX_QUEUE queue_u3_bt;
extern UART_HandleTypeDef huart3;

/* ----------------事件标志位---------------- */
#define BLE_EVT_CMD_OK          0x00000001
#define BLE_EVT_CMD_ERROR       0x00000002
#define BLE_EVT_CONN_SUCCESS    0x00000004
#define BLE_EVT_DISCONNECTED    0x00000008
#define BLE_EVT_WAIT_DATA       0x00000020  // 等待输入数据 (收到 '>')

/* ----------------类型定义---------------- */
// 数据接收回调函数类型
// data: 接收到的数据指针
// len: 数据长度
typedef void (*Ble_RxCallback_t)(uint8_t *data, uint16_t len);

/* ----------------控制块结构体---------------- */
typedef struct {
    TX_EVENT_FLAGS_GROUP    evt_flags;
    TX_MUTEX                lock;

    uint8_t                 line_buf[BLE_RX_BUF_SIZE];
    uint16_t                line_idx;

    volatile uint8_t        is_initialized;
    volatile uint8_t        is_connected;
    volatile uint8_t        is_spp_mode;    // 是否处于透传模式

    Ble_RxCallback_t        rx_callback;    // 应用层注册的接收回调

} Ble_Client_Ctrl_t;

extern Ble_Client_Ctrl_t ble_ctrl;

/* ----------------API 接口---------------- */
void Ble_Client_Init(void);
void Ble_Client_Task_Entry(ULONG thread_input);

// 注册接收回调
void Ble_RegisterRxCallback(Ble_RxCallback_t callback);

// 基础控制
UINT Ble_Init_Role(void);
UINT Ble_Start_Scan(void);
UINT Ble_Stop_Scan(void);
UINT Ble_Connect(const char* mac_addr);
UINT Ble_Disconnect(void);

// GATT 数据操作
UINT Ble_Gattc_DiscoverPrimaryService(void);
UINT Ble_Gattc_DiscoverChar(uint16_t srv_index);

/**
 * @brief 向特征值写入数据 (非透传)
 * @param srv_index 服务索引
 * @param char_index 特征值索引
 * @param data 数据指针
 * @param len 数据长度
 */
UINT Ble_Gattc_Write(uint16_t srv_index, uint16_t char_index, uint8_t *data, uint16_t len);

// 透传/SPP 操作
UINT Ble_Enter_SPP(void);
/**
 * @brief SPP透传模式发送数据
 */
UINT Ble_SPP_Send(uint8_t *data, uint16_t len);
UINT Ble_Exit_SPP(void); // 退出透传

#endif //ABOLUO_EXIT_BLE_CLIENT_H