/**
 * android_ble_client.c
 * NimBLE 1.7.0 Host Layer Implementation
 */

#include "ble_client.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* NimBLE Stack Headers */
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"

#define TAG "AndroidBLE"

/* -------------------------------------------------------------------------- */
/* 内部结构体与上下文                                                          */
/* -------------------------------------------------------------------------- */

// 上下文状态管理
typedef struct {
    uint16_t conn_handle;           // 当前连接句柄
    bool connected;                 // 连接状态
    android_ble_callbacks_t *cb;    // 应用层回调
} android_ble_context_t;

static android_ble_context_t g_ble_ctx = {
    .conn_handle = BLE_HS_CONN_HANDLE_NONE,
    .connected = false,
    .cb = NULL
};

/* -------------------------------------------------------------------------- */
/* 辅助函数                                                                   */
/* -------------------------------------------------------------------------- */

static void addr_to_str(const ble_addr_t *addr, char *dst) {
    const uint8_t *u8p = addr->val;
    sprintf(dst, "%02X:%02X:%02X:%02X:%02X:%02X",
            u8p[5], u8p[4], u8p[3], u8p[2], u8p[1], u8p[0]);
}

static void print_uuid(const ble_uuid_t *uuid) {
    char buf[BLE_UUID_STR_LEN];
    ble_uuid_to_str(uuid, buf);
    printf("%s", buf);
}

/* -------------------------------------------------------------------------- */
/* GATT 回调函数 (内部使用)                                                    */
/* -------------------------------------------------------------------------- */

// MTU 交换回调
static int on_mtu_exchanged(uint16_t conn_handle, const struct ble_gatt_error *error,
                            uint16_t mtu, void *arg) {
    if (g_ble_ctx.cb && g_ble_ctx.cb->on_mtu_changed) {
        g_ble_ctx.cb->on_mtu_changed(conn_handle, mtu, error->status);
    }
    return 0;
}

// 读操作回调
static int on_read_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                      struct ble_gatt_attr *attr, void *arg) {
    if (g_ble_ctx.cb && g_ble_ctx.cb->on_characteristic_read) {
        uint8_t *data = NULL;
        uint16_t len = 0;
        // 注意：这里简化处理，仅获取 mbuf 链的第一个块。
        // 如果数据很长，可能需要遍历 attr->om 链表。
        if (attr && attr->om) {
            data = attr->om->om_data;
            len = attr->om->om_len;
        }
        g_ble_ctx.cb->on_characteristic_read(conn_handle, error->status,
                                            attr ? attr->handle : 0, data, len);
    }
    return 0;
}

// 写操作回调
static int on_write_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                       struct ble_gatt_attr *attr, void *arg) {
    if (g_ble_ctx.cb && g_ble_ctx.cb->on_characteristic_write) {
        g_ble_ctx.cb->on_characteristic_write(conn_handle, error->status, attr ? attr->handle : 0);
    }
    return 0;
}

// 特征发现回调
static int on_chr_disc_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                          const struct ble_gatt_chr *chr, void *arg) {
    if (error->status == BLE_HS_EDONE) {
        // 单个服务的特征发现结束，我们这里简化逻辑，
        // 假设所有服务的特征发现都完成后再触发 on_services_discovered
        // 实际复杂的实现需要计数器或状态机
        if (g_ble_ctx.cb && g_ble_ctx.cb->on_services_discovered) {
            g_ble_ctx.cb->on_services_discovered(conn_handle, 0);
        }
        return 0;
    }

    if (error->status != 0) {
        printf("Error discovering characteristics: %d\n", error->status);
        return 0;
    }

    // 调试日志：打印发现的特征
    // printf("Discovered Characteristic: UUID=");
    // print_uuid(&chr->uuid.u);
    // printf(", Handle=%d\n", chr->val_handle);

    return 0;
}

// 服务发现回调
static int on_svc_disc_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                          const struct ble_gatt_svc *service, void *arg) {
    if (error->status == BLE_HS_EDONE) {
        return 0;
    }

    if (error->status != 0) {
        printf("Error discovering services: %d\n", error->status);
        return 0;
    }

    // printf("Discovered Service: UUID=");
    // print_uuid(&service->uuid.u);
    // printf("\n");

    // 发现服务后，立即递归发现该服务下的特征
    ble_gattc_disc_all_chrs(conn_handle, service->start_handle, service->end_handle,
                            on_chr_disc_cb, NULL);

    return 0;
}

/* -------------------------------------------------------------------------- */
/* GAP 事件处理 (连接、扫描、通知)                                             */
/* -------------------------------------------------------------------------- */

static int ble_gap_event(struct ble_gap_event *event, void *arg) {
    switch (event->type) {
        case BLE_GAP_EVENT_CONNECT:
            if (event->connect.status == 0) {
                g_ble_ctx.connected = true;
                g_ble_ctx.conn_handle = event->connect.conn_handle;
                if (g_ble_ctx.cb && g_ble_ctx.cb->on_connection_state_change) {
                    g_ble_ctx.cb->on_connection_state_change(event->connect.conn_handle, 0, 2); // 2 = Connected
                }
            } else {
                if (g_ble_ctx.cb && g_ble_ctx.cb->on_connection_state_change) {
                    g_ble_ctx.cb->on_connection_state_change(BLE_HS_CONN_HANDLE_NONE, event->connect.status, 0);
                }
            }
            break;

        case BLE_GAP_EVENT_DISCONNECT:
            g_ble_ctx.connected = false;
            g_ble_ctx.conn_handle = BLE_HS_CONN_HANDLE_NONE;
            if (g_ble_ctx.cb && g_ble_ctx.cb->on_connection_state_change) {
                g_ble_ctx.cb->on_connection_state_change(event->disconnect.conn.conn_handle, 0, 0); // 0 = Disconnected
            }
            break;

        case BLE_GAP_EVENT_DISC:
            if (g_ble_ctx.cb && g_ble_ctx.cb->on_scan_result) {
                char addr_str[20];
                addr_to_str(&event->disc.addr, addr_str);
                g_ble_ctx.cb->on_scan_result(addr_str, event->disc.rssi,
                                             (const uint8_t *)event->disc.data, event->disc.length_data);
            }
            break;

        case BLE_GAP_EVENT_NOTIFY_RX:
            if (g_ble_ctx.cb && g_ble_ctx.cb->on_characteristic_changed) {
                 uint8_t *data = NULL;
                 uint16_t len = 0;
                 if (event->notify_rx.om) {
                     data = event->notify_rx.om->om_data;
                     len = event->notify_rx.om->om_len;
                 }
                 g_ble_ctx.cb->on_characteristic_changed(event->notify_rx.conn_handle,
                                                         event->notify_rx.attr_handle,
                                                         data, len);
            }
            break;

        case BLE_GAP_EVENT_MTU:
            // 此事件通常由对端发起 MTU 交换时触发，若是主动请求，结果会在 on_mtu_exchanged 中返回
            // 这里可以处理被动更新的情况
            if (g_ble_ctx.cb && g_ble_ctx.cb->on_mtu_changed) {
                g_ble_ctx.cb->on_mtu_changed(event->mtu.conn_handle, event->mtu.value, 0);
            }
            break;
    }
    return 0;
}

/* -------------------------------------------------------------------------- */
/* API 实现                                                                   */
/* -------------------------------------------------------------------------- */

void android_ble_init(android_ble_callbacks_t *callbacks) {
    g_ble_ctx.cb = callbacks;
}

int android_ble_start_scan(void) {
    struct ble_gap_disc_params disc_params;

    // 参数配置：被动扫描，不启用白名单
    disc_params.filter_duplicates = 0;
    disc_params.passive = 0;
    disc_params.itvl = 0;
    disc_params.window = 0;
    disc_params.filter_policy = 0;
    disc_params.limited = 0;

    return ble_gap_disc(0, 2000, &disc_params, ble_gap_event, NULL);
}

int android_ble_stop_scan(void) {
    if (ble_gap_disc_active()) {
        return ble_gap_disc_cancel();
    }
    return 0;
}

int android_ble_connect(const char *addr_str, uint8_t addr_type) {
    ble_addr_t addr;
    unsigned int mac[6];

    // 简单的 MAC 解析
    if (sscanf(addr_str, "%2x:%2x:%2x:%2x:%2x:%2x",
               &mac[5], &mac[4], &mac[3], &mac[2], &mac[1], &mac[0]) != 6) {
        return BLE_HS_EINVAL;
    }

    for(int i=0; i<6; i++) {
        addr.val[i] = (uint8_t)mac[i];
    }
    addr.type = addr_type;

    return ble_gap_connect(0, &addr, BLE_HS_FOREVER, NULL, ble_gap_event, NULL);
}

int android_ble_disconnect(void) {
    if (!g_ble_ctx.connected) return BLE_HS_ENOTCONN;
    return ble_gap_terminate(g_ble_ctx.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
}

int android_ble_discover_services(void) {
    if (!g_ble_ctx.connected) return BLE_HS_ENOTCONN;
    return ble_gattc_disc_all_svcs(g_ble_ctx.conn_handle, on_svc_disc_cb, NULL);
}

int android_ble_read_char(uint16_t char_handle) {
    if (!g_ble_ctx.connected) return BLE_HS_ENOTCONN;
    return ble_gattc_read(g_ble_ctx.conn_handle, char_handle, on_read_cb, NULL);
}

int android_ble_write_char(uint16_t char_handle, const uint8_t *data, uint16_t len, int type) {
    if (!g_ble_ctx.connected) return BLE_HS_ENOTCONN;

    if (type == 1) { // No Response
        return ble_gattc_write_no_rsp_flat(g_ble_ctx.conn_handle, char_handle, data, len);
    } else { // With Response
        return ble_gattc_write_flat(g_ble_ctx.conn_handle, char_handle, data, len, on_write_cb, NULL);
    }
}

int android_ble_enable_notification(uint16_t cccd_handle, bool enable, bool is_indication) {
    if (!g_ble_ctx.connected) return BLE_HS_ENOTCONN;

    uint8_t value[2];
    value[0] = enable ? (is_indication ? 0x02 : 0x01) : 0x00;
    value[1] = 0x00;

    return ble_gattc_write_flat(g_ble_ctx.conn_handle, cccd_handle, value, sizeof(value), on_write_cb, NULL);
}

int android_ble_request_mtu(int mtu) {
    if (!g_ble_ctx.connected) return BLE_HS_ENOTCONN;
    return ble_gattc_exchange_mtu(g_ble_ctx.conn_handle, on_mtu_exchanged, NULL);
}