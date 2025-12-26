/*
* SPDX-FileCopyrightText: 2024 Espressif Systems (Shanghai) CO LTD
 *
 * Minimal Stub for SPI+BT Only Mode
 */

#include <stdio.h>
#include <string.h>
#include "esp_hosted_transport.h"
#include "port_esp_hosted_host_log.h"

static const char TAG[] = "stub";

typedef struct interface_buffer_handle_t interface_buffer_handle_t;

/* * 2. 最小化 API 入口
 * 初始化总线即可。
 */
extern void *bus_init_internal(void);
extern void bus_deinit_internal(void *bus_handle);

static void *g_bus_handle = NULL;

int esp_hosted_init(void)
{
    ESP_LOGI(TAG, "Initializing ESP Hosted (SPI + BT Only)...");

    // 初始化 SPI 总线
    g_bus_handle = bus_init_internal();

    if (!g_bus_handle) {
        ESP_LOGE(TAG, "Bus init failed");
        return -1;
    }

    ESP_LOGI(TAG, "Success. Ready for HCI.");
    return 0;
}

/* * 3. 补充一些可能被 linker 抱怨的符号 (可选)
 */
void esp_wifi_init(void) {} // 空桩