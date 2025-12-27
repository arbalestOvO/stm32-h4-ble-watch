//
// Created by 19571 on 2025/12/28.
//

#include "app_client.h"

#include <stdint.h>
#include <stdio.h>

#include "app_threadx.h"
#include "ble_client.h"
#include "stm32h7xx_hal.h"
#include "tx_api.h"

static TX_THREAD tx_app_thread;
static uint8_t thread_stack[1024];

uint8_t is_initialized = 0;

void App_Client_Task_Entry(ULONG entry_input) {
    printf("[MAIN] 开始扫描 (持续 10 秒)...\n");
    int rc = android_ble_start_scan();
    if (rc != 0) {
        printf("[MAIN] 扫描启动失败: %d\n", rc);
        return;
    }
    tx_thread_sleep(2000);
    printf("[MAIN] 停止扫描\n");
    android_ble_stop_scan();
}

void App_Client_Init() {
    if (is_initialized) {
        printf("App_Client_Init: is_initialized=%d\n", is_initialized);
        return;
    }
    if (tx_thread_create(&tx_app_thread, "app client thread", App_Client_Task_Entry, 0, thread_stack,
                        1024, TX_APP_THREAD_PRIO, TX_APP_THREAD_PREEMPTION_THRESHOLD,
                     TX_APP_THREAD_TIME_SLICE, TX_APP_THREAD_AUTO_START) != TX_SUCCESS)
    {
        is_initialized = 0;
        printf("App_Client_Init: tx_thread_create() failed\n");
    } else {
        is_initialized = 1;
    }
}

void App_Client_Destroy() {
    if (!is_initialized) {
        printf("App_Client_Destroy: is_initialized=%d\n", is_initialized);
        return;
    }
    if (tx_thread_delete(&tx_app_thread) != TX_SUCCESS) {
        printf("App_Client_Destroy: tx_thread_delete() failed\n");
    }
    is_initialized = 0;
}
