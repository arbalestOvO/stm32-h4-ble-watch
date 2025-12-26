//
// Created by 19571 on 2025/12/24.
//

#include "app_ble.h"

#include <stdio.h>

#include "app_threadx.h"
#include "ble_client.h"
#include "test_hci.h"
#include "tx_api.h"

void App_Ble_Client_Task_Entry(ULONG thread_input) {
    bt_test_start();
    return;
    if (Ble_Init_Role() != TX_SUCCESS) {
        printf("BLE Init Failed\n");
    }

    // 3. 开始扫描
    Ble_Start_Scan();

    // 模拟：扫描 5 秒
    tx_thread_sleep(5000 / 10); // tick conversion needed

    Ble_Stop_Scan();

    // 4. 连接设备 (假设已知 MAC)
    printf("Connecting...\n");
    if (Ble_Connect("C0:11:22:33:44:55") == TX_SUCCESS) {
        printf("Connected!\n");

        // 5. 发现服务
        Ble_Gattc_DiscoverPrimaryService();

        // 6. 发现第1个服务的特征值 (假设服务索引是1)
        Ble_Gattc_DiscoverChar(1);

        // 7. 进入透传模式进行大数据传输
        // Ble_Enter_SPP();

    } else {
        printf("Connect Failed\n");
    }

    while(1) {
        tx_thread_sleep(100);
    }
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
