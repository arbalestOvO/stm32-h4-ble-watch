#include <stdio.h>
#include <string.h>
#include <stdbool.h>

#include "ble_client.h"
#include "ble_hs_priv.h"
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "nimble/nimble_npl.h" // 引入 Event API

// --- 配置宏 ---
#define MAX_DISC_CHRS_PER_SVC 20
#define TARGET_SERVICE_UUID   0xFE86

// --- SDK 类型定义 (严格保持原样) ---



// --- 外部依赖声明 ---
extern void android_ble_init(android_ble_callbacks_t *callbacks);
extern int android_ble_connect(const char *addr_str, uint8_t addr_type);
extern int android_ble_request_mtu(int mtu);
extern int android_ble_enable_notification(uint16_t cccd_handle, bool enable, bool is_indication);
extern int android_ble_write_char(uint16_t char_handle, const uint8_t *data, uint16_t len, int type);

// --- 全局变量 ---
static uint16_t g_conn_handle = BLE_HS_CONN_HANDLE_NONE;
static uint16_t g_write_handle = 0;

// [修改] 由于 app_ble_svc_t 是 SDK 定义，不能添加自定义字段，
// 我们使用静态变量来维护“是否找到服务”的状态。
static app_ble_svc_t g_svc_cache = {0};
static bool g_is_service_found = false;

// [Event Objects] 定义所有异步操作的事件
static struct ble_npl_event g_mtu_evt;         // 用于请求 MTU
static struct ble_npl_event g_disc_svc_evt;    // 用于发现服务
static struct ble_npl_event g_disc_chr_evt;    // 用于发现特征
static struct ble_npl_event g_proc_result_evt; // 用于处理结果(订阅/写入)

// 前向声明
extern android_ble_callbacks_t g_test_callbacks;
static int on_scan_chrs_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                           const struct ble_gatt_chr *chr, void *arg);
static int on_disc_svc_cb(uint16_t conn_handle, const struct ble_gatt_error *error,
                          const struct ble_gatt_svc *service, void *arg);
int android_ble_discover_services_fixed(android_ble_callbacks_t *callbacks);

// --- [Event Handlers] 异步执行逻辑的核心 ---

/**
 * Handler 1: 执行 MTU 请求
 */
static void do_mtu_request_handler(struct ble_npl_event *ev) {
    uint16_t conn_handle = (uint16_t)(uintptr_t)ble_npl_event_get_arg(ev);
    printf("🔄 [Event] 正在请求 MTU (Conn: %d)...\n", conn_handle);
    android_ble_request_mtu(247);
}

/**
 * Handler 2: 执行服务发现
 */
static void do_discovery_service_handler(struct ble_npl_event *ev) {
    uint16_t conn_handle = (uint16_t)(uintptr_t)ble_npl_event_get_arg(ev);
    printf("🔄 [Event] 启动服务发现 (Conn: %d)...\n", conn_handle);
    android_ble_discover_services_fixed(&g_test_callbacks);
}

/**
 * Handler 3: 执行特征发现
 */
static void do_discovery_characteristic_handler(struct ble_npl_event *ev) {
    uint16_t conn_handle = (uint16_t)(uintptr_t)ble_npl_event_get_arg(ev);

    printf("🔄 [Event] 启动特征发现 (Range: 0x%04X - 0x%04X)...\n",
           g_svc_cache.start_handle, g_svc_cache.end_handle);

    int rc = ble_gattc_disc_all_chrs(conn_handle,
                                     g_svc_cache.start_handle,
                                     g_svc_cache.end_handle,
                                     on_scan_chrs_cb,
                                     &g_test_callbacks);
    if (rc != 0) {
        printf("❌ 发起特征扫描失败: %d\n", rc);
    }
}

/**
 * Handler 4: 处理发现结果（启用通知 + 写入数据）
 */
static void do_process_results_handler(struct ble_npl_event *ev) {
    // uint16_t conn_handle = (uint16_t)(uintptr_t)ble_npl_event_get_arg(ev); // 未使用
    printf("🔄 [Event] 处理发现结果 (订阅通知/写入数据)...\n");

    for (int i = 0; i < g_svc_cache.chr_count; i++) {
        const app_ble_chr_t *chr = &g_svc_cache.chars[i];

        // 1. 处理 Notify
        if (chr->properties & BLE_GATT_CHR_PROP_NOTIFY) {
            uint16_t cccd_handle = chr->val_handle + 1;
            printf("   -> [Event Action] 启用通知 (CCCD: 0x%04X)\n", cccd_handle);
            android_ble_enable_notification(cccd_handle, true, false);
        }

        // 2. 查找 Write Handle
        if (chr->properties & BLE_GATT_CHR_PROP_WRITE_NO_RSP) {
            g_write_handle = chr->val_handle;
            printf("   -> [Event Action] 找到写入特征 (Handle: 0x%04X)\n", g_write_handle);
        }
    }

    // 3. 执行写入
    if (g_write_handle != 0) {
        const char *msg = "Event Queue Full Fix";
        printf("🚀 [Event Action] 发送数据: %s\n", msg);
        android_ble_write_char(g_write_handle, (const uint8_t*)msg, strlen(msg), 1);
    }
}

// --- GATT 回调逻辑 (仅负责 Put Events) ---

static int on_scan_chrs_cb(uint16_t conn_handle,
                           const struct ble_gatt_error *error,
                           const struct ble_gatt_chr *chr,
                           void *arg) {
    android_ble_callbacks_t *callbacks = (android_ble_callbacks_t *)arg;

    if (error->status == BLE_HS_EDONE) {
        printf("✅ [GATT] 特征发现回调结束。通知上层...\n");
        if (callbacks && callbacks->on_services_discovered) {
            callbacks->on_services_discovered(conn_handle, 0, &g_svc_cache, 1);
        }
        return 0;
    }

    if (error->status != 0) {
        printf("❌ [GATT] 特征发现错误: %d\n", error->status);
        return 0;
    }

    if (chr != NULL && g_svc_cache.chr_count < MAX_DISC_CHRS_PER_SVC) {
        app_ble_chr_t *my_chr = &g_svc_cache.chars[g_svc_cache.chr_count];
        // [修改] 适配 SDK 结构体字段
        my_chr->def_handle = chr->def_handle;
        my_chr->val_handle = chr->val_handle;
        my_chr->properties = chr->properties;
        ble_uuid_copy((ble_uuid_any_t *)&my_chr->uuid, (const ble_uuid_t *)&chr->uuid);

        g_svc_cache.chr_count++;
    }
    return 0;
}

static int on_disc_svc_cb(uint16_t conn_handle,
                          const struct ble_gatt_error *error,
                          const struct ble_gatt_svc *service,
                          void *arg) {
    if (error->status == BLE_HS_EDONE) {
        // 使用静态变量判断
        if (g_is_service_found) {
            // [事件驱动] 服务发现结束 -> 触发特征发现事件
            tx_thread_sleep(30);
            ble_npl_event_init(&g_disc_chr_evt,
                               do_discovery_characteristic_handler,
                               (void *)(uintptr_t)conn_handle);
            ble_npl_eventq_put(ble_hs_evq_get(), &g_disc_chr_evt);
        }
        return 0;
    }

    if (service != NULL) {
        // [修改] 适配 SDK 结构体字段
        g_svc_cache.start_handle = service->start_handle;
        g_svc_cache.end_handle = service->end_handle;
        ble_uuid_copy((ble_uuid_any_t *)&g_svc_cache.uuid, (const ble_uuid_t *)&service->uuid);

        g_svc_cache.chr_count = 0;

        // 更新本地状态
        g_is_service_found = true;
    }
    return 0;
}

int android_ble_discover_services_fixed(android_ble_callbacks_t *callbacks) {
    memset(&g_svc_cache, 0, sizeof(g_svc_cache));
    g_is_service_found = false; // 重置本地状态

    ble_uuid16_t svc_uuid;
    ble_uuid_init_from_buf((ble_uuid_any_t *)&svc_uuid, (uint8_t[]){0x86, 0xFE}, 2);

    return ble_gattc_disc_svc_by_uuid(g_conn_handle,
                                      (const ble_uuid_t *)&svc_uuid,
                                      on_disc_svc_cb,
                                      callbacks);
}

// --- 测试回调逻辑 (Business Logic) ---
uint8_t times = 0;

static void test_on_connection_state_change(uint16_t conn_handle, int status, int new_state) {
    if (new_state == 2) { // Connected
        printf("✅ [Callback] 已连接。触发 MTU 事件...\n");
        times = 0;
        g_conn_handle = conn_handle;
        tx_thread_sleep(30);
        ble_npl_event_init(&g_mtu_evt,
                           do_mtu_request_handler,
                           (void *)(uintptr_t)conn_handle);
        ble_npl_eventq_put(ble_hs_evq_get(), &g_mtu_evt);

    } else if (new_state == 0) {
        printf("⚠️ [Callback] 已断开连接\n");
        g_conn_handle = BLE_HS_CONN_HANDLE_NONE;
    }
}


static void test_on_mtu_changed(uint16_t conn_handle, int mtu, int status) {
    if (status == 0 && times == 0) {
        times++;
        printf("✅ [Callback] MTU 更新完成。触发服务发现事件...\n");
        tx_thread_sleep(30);
        ble_npl_event_init(&g_disc_svc_evt,
                           do_discovery_service_handler,
                           (void *)(uintptr_t)conn_handle);
        ble_npl_eventq_put(ble_hs_evq_get(), &g_disc_svc_evt);
    }
}

static void test_on_services_discovered(uint16_t conn_handle, int status, const app_ble_svc_t *services, int svc_count) {
    printf("✅ [Callback] 服务发现完成。触发结果处理事件...\n");
    tx_thread_sleep(30);
    ble_npl_event_init(&g_proc_result_evt,
                       do_process_results_handler,
                       (void *)(uintptr_t)conn_handle);
    ble_npl_eventq_put(ble_hs_evq_get(), &g_proc_result_evt);
}

static void test_on_characteristic_changed(uint16_t conn_handle, uint16_t char_handle, const uint8_t *data, uint16_t len) {
    printf("📩 [Callback] 收到通知: %.*s\n", len, data);
}

static void test_on_characteristic_write(uint16_t conn_handle, int status, uint16_t char_handle) {
    printf("📝 [Callback] 写入回调 (Status: %d)\n", status);
}

android_ble_callbacks_t g_test_callbacks = {
    .on_connection_state_change = test_on_connection_state_change,
    .on_services_discovered = test_on_services_discovered,
    .on_characteristic_write = test_on_characteristic_write,
    .on_characteristic_changed = test_on_characteristic_changed,
    .on_mtu_changed = test_on_mtu_changed
};

void test(char* mac) {
    printf("========== BLE 客户端 (SDK适配版) ==========\n");
    android_ble_init(&g_test_callbacks);
    int rc = android_ble_connect(mac, 1);
    if (rc != 0) printf("连接请求失败: %d\n", rc);
}