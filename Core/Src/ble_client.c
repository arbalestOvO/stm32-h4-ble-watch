//
// Created by 19571 on 2025/12/24.
//

#include "ble_client.h"

#include <stdint.h>

#include "app_threadx.h"


Ble_Client_Ctrl_t ble_ctrl;

/* ================= 工具函数 ================= */

// 将单字节Hex字符转为数字 ('A'->10)
static uint8_t HexCharToByte(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    return 0;
}

// 将Hex字符串转为二进制数组 ("AA01" -> {0xAA, 0x01})
static void HexStrToBin(const char* hex, uint8_t* bin, uint16_t len) {
    for (uint16_t i = 0; i < len; i++) {
        bin[i] = (HexCharToByte(hex[i*2]) << 4) | HexCharToByte(hex[i*2 + 1]);
    }
}

/* ================= 核心驱动 ================= */

static TX_THREAD tx_app_thread;
static uint8_t thread_stack[1024];
void Ble_Client_Init(void) {

    if (tx_thread_create(&tx_app_thread, "ble core thread", Ble_Client_Task_Entry, 0, thread_stack,
                         512, TX_APP_THREAD_PRIO, TX_APP_THREAD_PREEMPTION_THRESHOLD,
                         TX_APP_THREAD_TIME_SLICE, TX_APP_THREAD_AUTO_START) != TX_SUCCESS)
    {
        return;
    }
    memset(&ble_ctrl, 0, sizeof(Ble_Client_Ctrl_t));
    tx_event_flags_create(&ble_ctrl.evt_flags, "BLE Flags");
    tx_mutex_create(&ble_ctrl.lock, "BLE Mutex", TX_NO_INHERIT);
}

void Ble_RegisterRxCallback(Ble_RxCallback_t callback) {
    ble_ctrl.rx_callback = callback;
}

// 通用命令发送 (内部使用)
static UINT Ble_SendCmd(const char* cmd, ULONG expect_evt, ULONG timeout_ms) {
    ULONG actual_flags;

    tx_mutex_get(&ble_ctrl.lock, TX_WAIT_FOREVER);
    tx_event_flags_set(&ble_ctrl.evt_flags, 0, TX_AND); // 清除标志

    HAL_UART_Transmit(&huart3, (uint8_t*)cmd, strlen(cmd), 100);
    HAL_UART_Transmit(&huart3, (uint8_t*)"\r\n", 2, 100);

    ULONG ticks = (timeout_ms * TX_TIMER_TICKS_PER_SECOND) / 1000;
    if(ticks == 0) ticks = 1;

    UINT status = tx_event_flags_get(&ble_ctrl.evt_flags, expect_evt | BLE_EVT_CMD_ERROR,
                                     TX_OR_CLEAR, &actual_flags, ticks);

    tx_mutex_put(&ble_ctrl.lock);

    if (status != TX_SUCCESS) return TX_NO_EVENTS;
    if (actual_flags & BLE_EVT_CMD_ERROR) return TX_WAIT_ERROR;
    return TX_SUCCESS;
}

/* ================= 接收任务与解析 ================= */

// 解析Notify数据: +BLEGATTCNOTIFY:0,1,1,4,AABBCCDD
static void Ble_Parse_Notify(char* line) {
    // 简单解析示例，实际需根据逗号分割
    // 假设格式固定，且最后一个参数是 Hex 数据
    char* data_ptr = strrchr(line, ',');
    if (data_ptr && ble_ctrl.rx_callback) {
        data_ptr++; // 跳过逗号
        uint16_t hex_len = strlen(data_ptr);
        if (hex_len % 2 == 0 && hex_len > 0) {
            uint8_t temp_buf[128]; // 临时缓冲
            uint16_t bin_len = hex_len / 2;
            if (bin_len > sizeof(temp_buf)) bin_len = sizeof(temp_buf);

            HexStrToBin(data_ptr, temp_buf, bin_len);
            ble_ctrl.rx_callback(temp_buf, bin_len);
        }
    }
}

static void Ble_Parse_Line(char* line) {
    if (strncmp(line, "OK", 2) == 0) {
        tx_event_flags_set(&ble_ctrl.evt_flags, BLE_EVT_CMD_OK, TX_OR);
    }
    else if (strncmp(line, "ERROR", 5) == 0) {
        tx_event_flags_set(&ble_ctrl.evt_flags, BLE_EVT_CMD_ERROR, TX_OR);
    }
    else if (line[0] == '>') { // 等待数据输入提示符
        tx_event_flags_set(&ble_ctrl.evt_flags, BLE_EVT_WAIT_DATA, TX_OR);
    }
    else if (strstr(line, "connected")) {
        ble_ctrl.is_connected = 1;
        tx_event_flags_set(&ble_ctrl.evt_flags, BLE_EVT_CONN_SUCCESS, TX_OR);
    }
    else if (strstr(line, "terminated")) {
        ble_ctrl.is_connected = 0;
        ble_ctrl.is_spp_mode = 0; // 断开连接自动退出SPP
        tx_event_flags_set(&ble_ctrl.evt_flags, BLE_EVT_DISCONNECTED, TX_OR);
    }
    // 处理Notify/Indicate数据
    else if (strstr(line, "+BLEGATTCNOTIFY:") || strstr(line, "+BLEGATTCIND:")) {
        Ble_Parse_Notify(line);
    }
}

void Ble_Client_Task_Entry(ULONG thread_input) {
    uint8_t rx_byte;

    while(1) {
        if (tx_queue_receive(&queue_u3_bt, &rx_byte, TX_WAIT_FOREVER) == TX_SUCCESS) {

            // --- SPP 模式处理 ---
            if (ble_ctrl.is_spp_mode) {
                // 在SPP模式下，直接透传给应用层
                // 为提高效率，可以积攒几个字节再回调，或者每字节回调
                if (ble_ctrl.rx_callback) {
                    ble_ctrl.rx_callback(&rx_byte, 1);
                }
                // 注意：这里需要一种机制检测退出透传的标志（如"+++"回复等），
                // 但通常退出由发送端控制，接收端只管收数据。
                continue;
            }

            // --- AT 模式处理 (解析 '\r\n') ---
            if (rx_byte == '>') {
                // 特殊处理 '>' 提示符，它可能没有回车换行
                ble_ctrl.line_buf[0] = '>';
                ble_ctrl.line_buf[1] = '\0';
                Ble_Parse_Line((char*)ble_ctrl.line_buf);
                ble_ctrl.line_idx = 0;
            }
            else if (rx_byte == '\n' || rx_byte == '\r') {
                if (ble_ctrl.line_idx > 0) {
                    ble_ctrl.line_buf[ble_ctrl.line_idx] = '\0';
                    Ble_Parse_Line((char*)ble_ctrl.line_buf);
                    ble_ctrl.line_idx = 0;
                }
            }
            else {
                if (ble_ctrl.line_idx < BLE_RX_BUF_SIZE - 1) {
                    ble_ctrl.line_buf[ble_ctrl.line_idx++] = rx_byte;
                }
            }
        }
    }
}

/* ================= 业务接口实现 ================= */

UINT Ble_Init_Role(void) { return Ble_SendCmd("AT+BLEINIT=1", BLE_EVT_CMD_OK, 2000); }
UINT Ble_Start_Scan(void) { return Ble_SendCmd("AT+BLESCAN=1", BLE_EVT_CMD_OK, 2000); }
UINT Ble_Stop_Scan(void) { return Ble_SendCmd("AT+BLESCAN=0", BLE_EVT_CMD_OK, 2000); }
UINT Ble_Gattc_DiscoverPrimaryService(void) { return Ble_SendCmd("AT+BLEGATTCPRIMSRV=0", BLE_EVT_CMD_OK, 2000); }
UINT Ble_Gattc_DiscoverChar(uint16_t srv_index) {
    char cmd[32]; snprintf(cmd, 32, "AT+BLEGATTCCHAR=0,%d", srv_index);
    return Ble_SendCmd(cmd, BLE_EVT_CMD_OK, 2000);
}

// 连接
UINT Ble_Connect(const char* mac_addr) {
    char cmd[64];
    snprintf(cmd, 64, "AT+BLECONN=0,\"%s\"", mac_addr);

    // 先等OK
    if (Ble_SendCmd(cmd, BLE_EVT_CMD_OK, 2000) != TX_SUCCESS) return TX_WAIT_ERROR;

    // 再等Connected事件 (最多10秒)
    ULONG actual;
    if (tx_event_flags_get(&ble_ctrl.evt_flags, BLE_EVT_CONN_SUCCESS, TX_OR, &actual,
        (10000 * TX_TIMER_TICKS_PER_SECOND)/1000) == TX_SUCCESS) {
        return TX_SUCCESS;
    }
    return TX_NO_EVENTS;
}

// GATT 写数据
UINT Ble_Gattc_Write(uint16_t srv_index, uint16_t char_index, uint8_t *data, uint16_t len) {
    char cmd[64];
    ULONG actual;

    tx_mutex_get(&ble_ctrl.lock, TX_WAIT_FOREVER);
    tx_event_flags_set(&ble_ctrl.evt_flags, 0, TX_AND);

    // 1. 发送写请求头: AT+BLEGATTCWR=0,srv,char,len
    snprintf(cmd, 64, "AT+BLEGATTCWR=0,%d,%d,%d", srv_index, char_index, len);
    HAL_UART_Transmit(&huart3, (uint8_t*)cmd, strlen(cmd), 100);
    HAL_UART_Transmit(&huart3, (uint8_t*)"\r\n", 2, 100);

    // 2. 等待 '>' 提示符
    UINT status = tx_event_flags_get(&ble_ctrl.evt_flags, BLE_EVT_WAIT_DATA | BLE_EVT_CMD_ERROR,
                                     TX_OR_CLEAR, &actual, 200); // 200ms等待提示符

    if (status == TX_SUCCESS && (actual & BLE_EVT_WAIT_DATA)) {
        // 3. 发送实际数据 (不带\r\n)
        HAL_UART_Transmit(&huart3, data, len, 100);

        // 4. 等待 OK
        status = tx_event_flags_get(&ble_ctrl.evt_flags, BLE_EVT_CMD_OK, TX_OR_CLEAR, &actual, 1000);
    } else {
        status = TX_WAIT_ERROR;
    }

    tx_mutex_put(&ble_ctrl.lock);
    return status;
}

// 进入 SPP 模式
UINT Ble_Enter_SPP(void) {
    if (Ble_SendCmd("AT+BLESPPCFG=1,1,1,1,1", BLE_EVT_CMD_OK, 2000) != TX_SUCCESS) {
        // 配置SPP参数(可选，根据手册)
    }

    if (Ble_SendCmd("AT+BLESPP", BLE_EVT_CMD_OK, 2000) == TX_SUCCESS) {
        ble_ctrl.is_spp_mode = 1; // 切换状态机模式
        return TX_SUCCESS;
    }
    return TX_WAIT_ERROR;
}

// SPP 发送 (直接透传)
UINT Ble_SPP_Send(uint8_t *data, uint16_t len) {
    if (!ble_ctrl.is_spp_mode) return TX_WAIT_ERROR;

    tx_mutex_get(&ble_ctrl.lock, TX_WAIT_FOREVER);
    HAL_UART_Transmit(&huart3, data, len, 500);
    tx_mutex_put(&ble_ctrl.lock);

    return TX_SUCCESS;
}

// 退出 SPP (通常是发送 +++ 并不带回车，具体看模块)
UINT Ble_Exit_SPP(void) {
    tx_mutex_get(&ble_ctrl.lock, TX_WAIT_FOREVER);
    HAL_UART_Transmit(&huart3, (uint8_t*)"+++", 3, 100);
    tx_thread_sleep(100); // 等待模块反应
    tx_mutex_put(&ble_ctrl.lock);

    // 发送AT测试看是否退出成功
    if (Ble_SendCmd("AT", BLE_EVT_CMD_OK, 1000) == TX_SUCCESS) {
        ble_ctrl.is_spp_mode = 0;
        return TX_SUCCESS;
    }
    return TX_WAIT_ERROR;
}