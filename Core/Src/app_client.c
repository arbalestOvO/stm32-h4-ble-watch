//
// Created by 19571 on 2025/12/28.
//

#include "app_client.h"

#include <stdint.h>
#include <stdio.h>

#include "app_threadx.h"
#include "auth_client.h"
#include "ble_client.h"
#include "stm32h7xx_hal.h"
#include "tx_api.h"
#include "host/ble_hs_id.h"

#define MAC_LEN 18
#define APP_CLIENT_QUEUE_STACK_SIZE 1024 * 4
// 计算需要的 Word 数量： (18 + 3) / 4 = 5 Words
// 这样 5 * 4 = 20 字节，足够存下 18 字节的 MAC
#define QUEUE_MSG_SIZE_WORDS  ((MAC_LEN + 3) / 4)

// 队列总缓存区大小（假设你想存 10 个消息）
#define QUEUE_CAPACITY 10
uint8_t queue_stack[QUEUE_CAPACITY * QUEUE_MSG_SIZE_WORDS * 4];
uint8_t notify_stack[QUEUE_CAPACITY * QUEUE_MSG_SIZE_WORDS * 4];

static TX_THREAD tx_app_thread;
static uint8_t thread_stack[APP_CLIENT_QUEUE_STACK_SIZE];
TX_QUEUE app_client_queue;
TX_QUEUE app_connect_notify_queue;

uint8_t is_initialized = 0;

void App_Client_Task_Entry(ULONG entry_input) {
    while (1) {
        char mac[MAC_LEN];
        tx_queue_receive(&app_client_queue, mac, TX_WAIT_FOREVER);
        printf("android_ble_connect mac %s\n", mac);
        uint8_t own_addr_type;
        int rc = ble_hs_id_infer_auto(0, &own_addr_type);
        if (rc != 0) {
            printf("Error determining own address type: %d\n", rc);
            continue;
        }
        int ret = android_ble_connect(mac, own_addr_type);
        printf("android_ble_connect returned %d\n", ret);
        uint32_t connect_allow = 0;
        tx_queue_receive(&app_connect_notify_queue, &connect_allow, TX_WAIT_FOREVER);
        if (connect_allow == 0) continue;
        AuthContext_t *ctx = AuthContext_Create(mac, 10000, 2);
        if (ctx == NULL) continue;
        AuthResult_t auth_result = auth(ctx);
        printf("auth_result returned %d\n", auth_result);
        AuthContext_Free(ctx);
    }
}

void App_Client_Init() {
    if (is_initialized) {
        printf("App_Client_Init: is_initialized=%d\n", is_initialized);
        return;
    }
    int ret = tx_queue_create(&app_client_queue, "App Connect Queue", QUEUE_MSG_SIZE_WORDS, // 接受来自ui的连接通知
                    queue_stack, sizeof(queue_stack));
    if (ret != TX_SUCCESS) {
        printf("App_Client_Init: tx_queue_create() failed %d\n", ret);
        return;
    }

    ret = tx_queue_create(&app_connect_notify_queue, "App Connect Notify Queue", 1, // 接受来自ui的连接通知
                    notify_stack, sizeof(notify_stack));
    if (ret != TX_SUCCESS) {
        printf("App_Client_Init: tx_queue_create() failed %d\n", ret);
        return;
    }
    if (tx_thread_create(&tx_app_thread, "app client thread", App_Client_Task_Entry, 0, thread_stack,
                        APP_CLIENT_QUEUE_STACK_SIZE, TX_APP_THREAD_PRIO, TX_APP_THREAD_PREEMPTION_THRESHOLD,
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
    // 1. 先强制终止线程 (无论它当前在做什么)
    // 这一步会让线程停止运行，并将状态变为 TX_TERMINATED
    int status = tx_thread_terminate(&tx_app_thread);

    if (status == TX_SUCCESS) {
        // 2. 只有终止成功后，才能删除线程控制块
        status = tx_thread_delete(&tx_app_thread);

        if (status == TX_SUCCESS) {
            printf("Thread deleted successfully.\n");
            // 注意：这里通常还需要释放该线程使用的堆栈内存 (如果原本是 malloc 出来的)
        } else {
            printf("Delete failed: 0x%02X\n", status);
        }
    } else {
        // 如果终止失败 (例如线程已经是 terminated，虽然 terminate 会返回 success，但需注意其他错误)
        printf("Terminate failed: 0x%02X\n", status);
    }
    tx_queue_delete(&app_client_queue);
    is_initialized = 0;
}
