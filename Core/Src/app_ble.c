//
// Created by 19571 on 2025/12/24.
//

#include "app_ble.h"

#include <stdio.h>

#include "app_client.h"
#include "app_threadx.h"
#include "ble_client.h"
#include "esp_hosted.h"
#include "test_hci.h"
#include "tx_api.h"
#include "host/ble_hs.h"
#include "host/ble_sm.h"
#include "host/util/util.h"
#include "nimble/nimble_port.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"

void ble_store_ram_init(void);


void on_scan_result(const char *addr_str, int rssi, const uint8_t *adv_data, int adv_len);

void on_connection_state_change(uint16_t conn_handle, int status, int new_state);

void on_services_discovered(uint16_t conn_handle, int status);

void on_characteristic_read(uint16_t conn_handle, int status, uint16_t char_handle, const uint8_t *data, uint16_t len);

void on_characteristic_write(uint16_t conn_handle, int status, uint16_t char_handle);

void on_characteristic_changed(uint16_t conn_handle, uint16_t char_handle, const uint8_t *data, uint16_t len);

void on_mtu_changed(uint16_t conn_handle, int mtu, int status);

android_ble_callbacks_t callbacks = {
    .on_scan_result = on_scan_result,
    .on_connection_state_change = on_connection_state_change,
    .on_services_discovered = on_services_discovered,
    .on_characteristic_read = on_characteristic_read,
    .on_characteristic_write = on_characteristic_write,
    .on_characteristic_changed = on_characteristic_changed,
    .on_mtu_changed = on_mtu_changed,
};

#define BLE_AD_TYPE_SHORT_NAME    0x08
#define BLE_AD_TYPE_COMPLETE_NAME 0x09

int parse_device_name(const uint8_t *data, int len, char *out_name, int out_size) {
    int i = 0;

    // 初始化输出为空字符串
    if (out_size > 0) out_name[0] = '\0';

    while (i < len) {
        uint8_t length = data[i]; // 当前 AD Structure 的长度

        // 1. 安全检查：如果长度为0，说明后面全是填充数据的0，直接结束
        if (length == 0) break;

        // 2. 安全检查：防止数组越界读取
        if (i + length + 1 > len) break;

        uint8_t type = data[i + 1]; // 数据类型

        // 3. 判断是否为名称类型
        if (type == BLE_AD_TYPE_COMPLETE_NAME || type == BLE_AD_TYPE_SHORT_NAME) {
            // 数据长度 = 总长度(length) - 类型占用的1字节
            int name_len = length - 1;

            // 限制拷贝长度，防止缓冲区溢出
            if (name_len >= out_size) {
                name_len = out_size - 1;
            }

            // 拷贝名称数据 (注意：i+2 是跳过 length 和 type 字节)
            memcpy(out_name, &data[i + 2], name_len);
            out_name[name_len] = '\0'; // 确保字符串以 null 结尾
            return 1; // 找到并返回
        }

        // 4. 跳转到下一个 AD Structure
        // 这里的 +1 是指跳过开头的 Length 字节本身
        i += length + 1;
    }

    return 0; // 未找到名称
}

void on_scan_result(const char *addr_str, int rssi, const uint8_t *adv_data, int adv_len) {
    char name_buf[64] = {0}; // 准备一个缓冲区存放名字
    printf("[APP] 发现设备: Addr=%s, RSSI=%d, DataLen=%d\n", addr_str, rssi, adv_len);
    if (parse_device_name(adv_data, adv_len, name_buf, sizeof(name_buf))) {
        printf("[APP] 发现设备: Addr=%s, RSSI=%d, Name=%s\n", addr_str, rssi, name_buf);
    } else {
        printf("[APP] 发现设备: Addr=%s, RSSI=%d, Name=(Unknown)\n", addr_str, rssi);
    }
}

void on_connection_state_change(uint16_t conn_handle, int status, int new_state) {
    printf("[APP] 连接状态改变: Handle=%d, Status=%d, NewState=%d\n", conn_handle, status, new_state);
}

void on_services_discovered(uint16_t conn_handle, int status) {
    printf("[APP] 服务发现: %d\n", status);
}

void on_characteristic_read(uint16_t conn_handle, int status, uint16_t char_handle, const uint8_t *data, uint16_t len) {
    printf("[APP] 读取回调 (Handle=%d, Status=%d, Len=%d): ", char_handle, status, len);
}

void on_characteristic_write(uint16_t conn_handle, int status, uint16_t char_handle) {
    printf("[APP] 写入完成 (Handle=%d, Status=%d)\n", char_handle, status);
}

void on_characteristic_changed(uint16_t conn_handle, uint16_t char_handle, const uint8_t *data, uint16_t len) {
    printf("[APP] 收到通知 (Handle=%d): ", char_handle);
}

void on_mtu_changed(uint16_t conn_handle, int mtu, int status) {
    printf("[APP] MTU 更新: %d (Status=%d)\n", mtu, status);
}

static void print_addr(const void *addr) {
    const uint8_t *u8p;
    u8p = addr;
    printf("%02x:%02x:%02x:%02x:%02x:%02x",
                   u8p[5], u8p[4], u8p[3], u8p[2], u8p[1], u8p[0]);
}

static void on_sync(void) {
    int rc;

    /* 确保地址已经生成 */
    rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        printf("错误: 加载地址失败\n");
        return;
    }

    /* 打印我们的 Public Address (如果有) */
    uint8_t addr_val[6] = {0};
    rc = ble_hs_id_copy_addr(BLE_ADDR_PUBLIC, addr_val, NULL);
    if (rc == 0) {
        printf("Device Address: ");
        print_addr(addr_val);
        printf("\n");
    }
    android_ble_init(&callbacks);
    App_Client_Init();
}

static void on_reset(int reason) {
    printf("Resetting state; reason=%d\n", reason);
    App_Client_Destroy();
}

void App_Ble_Client_Task_Entry(ULONG thread_input) {
    esp_hosted_init();
    /* 确保总线已经初始化 (esp_hosted_init 已被调用) */

    /* 延迟 2 秒等待 ESP32 完全启动 */
    tx_thread_sleep(2000);
    // bt_test_send_reset();
    /* 1. 初始化 NimBLE 端口 (初始化内存、OS资源) */
    nimble_port_init();

    /* 2. 初始化必要的服务 (GAP/GATT 是必须的，否则容易出错) */
    ble_svc_gap_init();
    ble_svc_gatt_init();

    /* 3. 配置回调函数 */
    ble_hs_cfg.reset_cb = on_reset;
    ble_hs_cfg.sync_cb = on_sync;

    /* 配置 IO 能力 (通常扫描不需要，但如果是作为从机需要) */
    ble_hs_cfg.sm_io_cap = BLE_SM_IO_CAP_NO_IO;
    ble_store_ram_init();
    nimble_port_run();
}

static TX_THREAD tx_app_thread;
static uint8_t thread_stack[1024];

void App_BLE_Init() {
    if (tx_thread_create(&tx_app_thread, "app ble client thread", App_Ble_Client_Task_Entry, 0, thread_stack,
                            1024, TX_APP_THREAD_PRIO, TX_APP_THREAD_PREEMPTION_THRESHOLD,
                         TX_APP_THREAD_TIME_SLICE, TX_APP_THREAD_AUTO_START) != TX_SUCCESS)
    {
        printf("App_BLE_Init: tx_thread_create() failed\n");
    }
}
