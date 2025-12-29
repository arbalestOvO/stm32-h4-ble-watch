/**
 * android_ble_client.c
 * NimBLE 1.7.0 Host Layer Implementation
 */

#include "ble_client.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

/* NimBLE Stack Headers */
#include "ble_hs_priv.h"
#include "ui_interface.h"
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


// 定义目标服务 UUID: 0xFE86
static const ble_uuid16_t svc_uuid_fe86 = BLE_UUID16_INIT(0xFE86);

// 前向声明回调
static int on_fe86_chars_found(uint16_t conn_handle, const struct ble_gatt_error *error,
                               const struct ble_gatt_chr *chr, void *arg);
static int on_fe86_svc_found(uint16_t conn_handle, const struct ble_gatt_error *error,
                             const struct ble_gatt_svc *service, void *arg);

typedef void (*on_services_discovered_fn)(uint16_t conn_handle,
                                          int status,
                                          const app_ble_svc_t *services,
                                          int svc_count);

// 用于管理发现过程的上下文
typedef struct {
    // 用户提供的回调函数
    on_services_discovered_fn user_cb;

    // 暂存发现结果的缓冲区
    app_ble_svc_t temp_svc;

    // 标记是否真的找到了服务（用于处理 EDONE 但没数据的情况）
    bool service_found;
} disc_context_t;

static disc_context_t g_fe86_ctx;

// 清空上下文的辅助函数
static void reset_ctx() {
    memset(&g_fe86_ctx, 0, sizeof(g_fe86_ctx));
}

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

static int internal_on_chars_found(uint16_t conn_handle,
                                   const struct ble_gatt_error *error,
                                   const struct ble_gatt_chr *chr,
                                   void *arg) {
    // 1. 结束判断 (EDONE)
    if (error->status == BLE_HS_EDONE) {
        printf("✅ [GATT] 0xFE86 特征发现完毕，回调上层应用\n");

        // 调用用户的回调接口！
        if (g_fe86_ctx.user_cb) {
            // 参数：conn_handle, status=0, 服务数组指针, 服务数量=1
            g_fe86_ctx.user_cb(conn_handle, 0, &g_fe86_ctx.temp_svc, 1);
        }

        // 任务完成，清理上下文（可选）
        reset_ctx();
        return 0;
    }

    // 2. 错误处理
    if (error->status != 0) {
        printf("❌ [GATT] 特征发现出错: %d\n", error->status);
        if (g_fe86_ctx.user_cb) {
            g_fe86_ctx.user_cb(conn_handle, error->status, NULL, 0);
        }
        reset_ctx();
        return 0;
    }

    // 3. 填充数据到你的结构体
    if (chr != NULL) {
        app_ble_svc_t *svc = &g_fe86_ctx.temp_svc;

        // 检查数组是否满了
        if (svc->chr_count < MAX_DISC_CHRS_PER_SVC) {
            app_ble_chr_t *my_chr = &svc->chars[svc->chr_count];

            // 复制 Handle 和 属性
            my_chr->def_handle = chr->def_handle;
            my_chr->val_handle = chr->val_handle;
            my_chr->properties = chr->properties;

            // 复制 UUID (NimBLE 提供了专用复制函数)
            ble_uuid_copy((ble_uuid_any_t *)&my_chr->uuid, (const ble_uuid_t *)&chr->uuid);

            svc->chr_count++;

            // [调试打印]
            char buf[BLE_UUID_STR_LEN];
            ble_uuid_to_str((const ble_uuid_t *)&chr->uuid, buf);
            printf("   -> 存入特征: %s (Handle 0x%04X)\n", buf, chr->val_handle);
        } else {
            printf("⚠️ 警告: 特征数量超过 MAX_DISC_CHRS_PER_SVC，忽略剩余特征\n");
        }
    }

    return 0;
}


static int internal_on_svc_found(uint16_t conn_handle,
                                 const struct ble_gatt_error *error,
                                 const struct ble_gatt_svc *service,
                                 void *arg) {
    // 1. 错误处理
    if (error->status != 0) {
        // 如果是 EDONE 且没找到服务
        if (error->status == BLE_HS_EDONE) {
            if (!g_fe86_ctx.service_found) {
                printf("❌ [GATT] 未找到 0xFE86 服务\n");
                if (g_fe86_ctx.user_cb) {
                    // status 非 0 表示失败
                    g_fe86_ctx.user_cb(conn_handle, BLE_HS_ENOENT, NULL, 0);
                }
                reset_ctx();
            }
            return 0;
        }

        // 其他错误
        printf("❌ [GATT] 服务发现出错: %d\n", error->status);
        if (g_fe86_ctx.user_cb) {
            g_fe86_ctx.user_cb(conn_handle, error->status, NULL, 0);
        }
        reset_ctx();
        return 0;
    }

    // 2. 找到了服务
    if (service != NULL) {
        printf("✅ [GATT] 找到 0xFE86 (Handle %d ~ %d)，开始搜索特征...\n",
               service->start_handle, service->end_handle);

        g_fe86_ctx.service_found = true;

        // 填充服务的基本信息
        g_fe86_ctx.temp_svc.start_handle = service->start_handle;
        g_fe86_ctx.temp_svc.end_handle = service->end_handle;
        g_fe86_ctx.temp_svc.chr_count = 0; // 清零特征计数
        ble_uuid_copy((ble_uuid_any_t *)&g_fe86_ctx.temp_svc.uuid, (const ble_uuid_t *)&service->uuid);

        // 3. 【关键】立即发起特征发现
        // 注意：这里只搜这个服务范围内的特征，效率极高
        int rc = ble_gattc_disc_all_chrs(
            conn_handle,
            service->start_handle,
            service->end_handle,
            internal_on_chars_found, // 下一步的回调
            NULL
        );

        if (rc != 0) {
            printf("❌ 发起特征搜索失败: %d\n", rc);
            if (g_fe86_ctx.user_cb) {
                g_fe86_ctx.user_cb(conn_handle, rc, NULL, 0);
            }
            reset_ctx();
        }
    }

    return 0;
}

/* -------------------------------------------------------------------------- */
/* GAP 事件处理 (连接、扫描、通知)                                             */
/* -------------------------------------------------------------------------- */
static int ble_gap_event(struct ble_gap_event *event, void *arg) {
    printf("event %d\n", event->type);
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
            ui_show_notify_safe(false, false, "设备已断连");
            break;

        case BLE_GAP_EVENT_DISC:
            if (g_ble_ctx.cb && g_ble_ctx.cb->on_scan_result) {
                char addr_str[20];
                addr_to_str(&event->disc.addr, addr_str);
                g_ble_ctx.cb->on_scan_result(addr_str, event->disc.rssi,
                                             (const uint8_t *)event->disc.data, event->disc.length_data);
            }
            break;
        case BLE_GAP_EVENT_DISC_COMPLETE:
            ui_show_notify_safe(false, false, "扫描结束");
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
    struct ble_gap_disc_params disc_params = {0};
    ui_show_notify_safe(true, false, "正在扫描中");
    // 参数配置：被动扫描，不启用白名单
    disc_params.filter_duplicates = 1; // 过滤重复包
    disc_params.passive = 0;           // 主动扫描
    disc_params.itvl = 0;              // 使用默认间隔
    disc_params.window = 0;            // 使用默认窗口
    uint8_t own_addr_type;
    int rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        printf("Error determining own address type; rc=%d\n", rc);
        return -1;
    }

    // 打印看看计算出了什么
    printf("Calculated own_addr_type: %d\n", own_addr_type);
    return ble_gap_disc(own_addr_type, 5000, &disc_params, ble_gap_event, NULL);
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
    printf("android connect: %s\n", addr_str);
    return ble_gap_connect(0, &addr, 10000, NULL, ble_gap_event, NULL);
}

int android_ble_disconnect(void) {
    if (!g_ble_ctx.connected) return BLE_HS_ENOTCONN;
    return ble_gap_terminate(g_ble_ctx.conn_handle, BLE_ERR_REM_USER_CONN_TERM);
}

static int on_scan_chrs_from_middle(uint16_t conn_handle,
                                    const struct ble_gatt_error *error,
                                    const struct ble_gatt_chr *chr,
                                    void *arg) {
    // 1. 结束判断 (EDONE)
    if (error->status == BLE_HS_EDONE) {
        printf("✅ [GATT] 0xFE86 特征发现完毕，回调上层应用\n");

        // 调用用户的回调接口！
        if (g_fe86_ctx.user_cb) {
            // 参数：conn_handle, status=0, 服务数组指针, 服务数量=1
            g_fe86_ctx.user_cb(conn_handle, 0, &g_fe86_ctx.temp_svc, 1);
        }

        // 任务完成，清理上下文（可选）
        reset_ctx();
        return 0;
    }

    // 2. 错误处理
    if (error->status != 0) {
        printf("❌ [GATT] 特征发现出错: %d\n", error->status);
        if (g_fe86_ctx.user_cb) {
            g_fe86_ctx.user_cb(conn_handle, error->status, NULL, 0);
        }
        reset_ctx();
        return 0;
    }

    // 3. 填充数据到你的结构体
    if (chr != NULL) {
        app_ble_svc_t *svc = &g_fe86_ctx.temp_svc;

        // 检查数组是否满了
        if (svc->chr_count < MAX_DISC_CHRS_PER_SVC) {
            app_ble_chr_t *my_chr = &svc->chars[svc->chr_count];

            // 复制 Handle 和 属性
            my_chr->def_handle = chr->def_handle;
            my_chr->val_handle = chr->val_handle;
            my_chr->properties = chr->properties;

            // 复制 UUID (NimBLE 提供了专用复制函数)
            ble_uuid_copy((ble_uuid_any_t *)&my_chr->uuid, (const ble_uuid_t *)&chr->uuid);

            svc->chr_count++;

            // [调试打印]
            char buf[BLE_UUID_STR_LEN];
            ble_uuid_to_str((const ble_uuid_t *)&chr->uuid, buf);
            printf("   -> 存入特征: %s (Handle 0x%04X)\n", buf, chr->val_handle);
        } else {
            printf("⚠️ 警告: 特征数量超过 MAX_DISC_CHRS_PER_SVC，忽略剩余特征\n");
        }
    }

    return 0;
}

int android_ble_discover_services(void) {
    if (!g_ble_ctx.connected) return BLE_HS_ENOTCONN;
    // return ble_gattc_disc_all_svcs(g_ble_ctx.conn_handle, on_svc_disc_cb, NULL);

    g_fe86_ctx.user_cb = g_ble_ctx.cb->on_services_discovered;
    g_fe86_ctx.service_found = false;
    static const ble_uuid16_t uuid_fe86 = BLE_UUID16_INIT(0xFE86);
    reset_ctx();
    return ble_gattc_disc_all_chrs(
        g_ble_ctx.conn_handle,
        0x0012,   // <--- 手动指定起点 (Page 2)
        0xFFFF,
        on_scan_chrs_from_middle,
        NULL
    );
}

int android_ble_read_char(uint16_t char_handle) {
    if (!g_ble_ctx.connected) return BLE_HS_ENOTCONN;
    return ble_gattc_read(g_ble_ctx.conn_handle, char_handle, on_read_cb, NULL);
}

int android_ble_write_char(uint16_t char_handle, const uint8_t *data, uint16_t len, int type) {
    if (!g_ble_ctx.connected) return BLE_HS_ENOTCONN;
    printf("SEND RAW: ");
    for (int i = 0;i < len;i++) {
        printf(" %02X", data[i]);
    }
    printf("\n");
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