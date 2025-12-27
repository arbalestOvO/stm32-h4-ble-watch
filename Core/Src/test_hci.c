//
// Created by 19571 on 2025/12/27.
//

#include "test_hci.h"

/*
 * SPDX-FileCopyrightText: 2024 User
 *
 * ESP-Hosted SPI + Bluetooth Loopback Test
 */

#include <stdio.h>
#include <string.h>

#include "esp_hosted.h"
#include "main.h"
#include "port_esp_hosted_host_log.h"
#include "esp_hosted_transport.h" // 包含 esp_hosted_tx 定义
#include "port_esp_hosted_host_os.h"
#include "esp_hosted_interface.h"
#include "tx_api.h"
/* NimBLE 头文件 */
#include "nimble/ble.h"
#include "host/ble_hs.h"
#include "host/util/util.h"
#include "services/gap/ble_svc_gap.h"

/* 引入你之前定义的 console (或者直接用 printf) */
#include "console/console.h"
#include "nimble/nimble_port.h"
#include "services/gatt/ble_svc_gatt.h"


static const char TAG[] = "BT_TEST";

/* HCI 命令结构：OpCode(2bytes) + ParamLen(1byte) + Params */
/* HCI Reset Command: OGF=0x03, OCF=0x0003 -> OpCode = 0x0C03 */
static const uint8_t hci_reset_cmd[] = { 0x01, 0x03, 0x0C, 0x00 };

/* 外部声明 */
extern int esp_hosted_tx(uint8_t iface_type, uint8_t iface_num,
        uint8_t *payload_buf, uint16_t payload_len, uint8_t buff_zcopy,
        uint8_t *buffer_to_free, void (*free_buf_func)(void *ptr), uint8_t flags);

void ble_store_ram_init(void);

/* ----------------------------------------------------------------
 * 辅助函数：打印 MAC 地址
 * ---------------------------------------------------------------- */
void print_addr(const void *addr);

/* ----------------------------------------------------------------
 * GAP 事件回调：处理扫描到的结果
 * ---------------------------------------------------------------- */
static int gap_event(struct ble_gap_event *event, void *arg) {
    console_printf("event: %d\n", event->type);
    switch (event->type) {

        /* 发现设备事件 (每收到一个广播包触发一次) */
        case BLE_GAP_EVENT_DISC:
            console_printf("收到广播: [MAC: ");
            print_addr(event->disc.addr.val);
            console_printf("] [RSSI: %d] [Type: %d] [Len: %d]\n",
                           event->disc.rssi,
                           event->disc.event_type,
                           event->disc.length_data);
            return 0;

            /* 扫描结束事件 */
        case BLE_GAP_EVENT_DISC_COMPLETE:
            console_printf("扫描结束 (原因: %d)\n", event->disc_complete.reason);
            return 0;

        default:
            return 0;
    }
}

/* ----------------------------------------------------------------
 * 扫描函数
 * ---------------------------------------------------------------- */
static void scan(void) {
    uint8_t own_addr_type;
    struct ble_gap_disc_params disc_params;
    int rc;

    /* 1. 确定我们自己的地址类型 (通常是 Public 或 Random Static) */
    /* 尝试推导最佳地址类型 */
    rc = ble_hs_id_infer_auto(0, &own_addr_type);
    if (rc != 0) {
        console_printf("错误: 无法获取自身地址类型; rc=%d\n", rc);
        return;
    }

    /* 2. 配置扫描参数 */
    memset(&disc_params, 0, sizeof(disc_params));
    disc_params.filter_duplicates = 1; /* 过滤重复包：设为 1 则同一个设备只报一次，设为 0 则一直报 */
    disc_params.passive = 1;           /* 被动扫描 (只听不问)，最简单 */
    disc_params.itvl = 0;              /* 0 表示使用默认值 */
    disc_params.window = 0;            /* 0 表示使用默认值 */

    /* 3. 开始扫描 (持续时间: BLE_HS_FOREVER 表示一直扫) */
    rc = ble_gap_disc(own_addr_type, 5000, &disc_params, gap_event, NULL);
    if (rc != 0) {
        console_printf("错误: 启动扫描失败; rc=%d\n", rc);
    } else {
        console_printf("成功: 开始扫描...\n");
    }
}


static void on_sync(void) {
    int rc;

    /* 确保地址已经生成 */
    rc = ble_hs_util_ensure_addr(0);
    if (rc != 0) {
        console_printf("错误: 加载地址失败\n");
        return;
    }

    /* 打印我们的 Public Address (如果有) */
    uint8_t addr_val[6] = {0};
    rc = ble_hs_id_copy_addr(BLE_ADDR_PUBLIC, addr_val, NULL);
    if (rc == 0) {
        console_printf("Device Address: ");
        print_addr(addr_val);
        console_printf("\n");
    }
}

/* ----------------------------------------------------------------
 * 复位回调：Controller 复位时调用
 * ---------------------------------------------------------------- */
static void on_reset(int reason) {
    console_printf("Resetting state; reason=%d\n", reason);
}


/* 简单的内存释放回调 */
static void my_free_func(void *ptr) {
    if (ptr) {
        free(ptr);
    }
}

void nimble_host_task_entry(ULONG input) {
    console_printf("NimBLE Host Task Started\n");

    /* 进入 NimBLE 事件循环，这个函数不会返回 */
    nimble_port_run();
}

void bt_test_send_reset(void)
{
    ESP_LOGI(TAG, ">>> Sending HCI Reset Command...");

    /* 1. 分配内存 (ESP-Hosted 要求 payload 必须在堆上，或者它是静态的但不被释放) */
    /* 为了安全起见，我们模拟一次标准的 malloc 过程 */
    uint8_t *buf = (uint8_t *)malloc(sizeof(hci_reset_cmd));
    if (!buf) {
        ESP_LOGE(TAG, "Malloc failed");
        return;
    }
    memcpy(buf, hci_reset_cmd, sizeof(hci_reset_cmd));

    /* 2. 发送数据
     * iface_type = ESP_HCI_IF (蓝牙通道)
     * iface_num  = 0
     * payload_len = 4
     * buff_zcopy = 0 (让驱动去拷贝，或者我们可以传 1 并自己管理生命周期)
     * 这里我们用非零拷贝模式，让驱动拷贝一份，我们可以立即释放或者交给它的回调释放
     */

    /* 注意：根据 spi_drv.c 的实现，如果我们设置 buff_zcopy=0，驱动会申请新内存并拷贝。
     * 如果设置 buff_zcopy=1，驱动会直接使用我们的指针，并在发送完后调用 free_buf_func。
     * 我们使用 Zero Copy 模式以测试完整的内存生命周期。
     */
    int ret = esp_hosted_tx(ESP_HCI_IF, 0,
                            buf, sizeof(hci_reset_cmd),
                            0,      /* Zero Copy = 1 */
                            buf,    /* Buffer to free */
                            my_free_func, /* Free function */
                            0);     /* Flags */
    if (ret != 0) {
        ESP_LOGE(TAG, "Failed to send HCI packet: %d", ret);
        free(buf); // 发送失败需手动释放
    } else {
        ESP_LOGI(TAG, "HCI Packet Sent to Queue. Waiting for response...");
    }
}



/* 在你的主任务或初始化完成后调用这个函数 */
void bt_test_start(void)
{
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
    nimble_host_task_entry(0);
    // while (1) {
    //     bt_test_send_reset();
    //     /* 每 5 秒发一次，方便示波器抓波形 */
    //     tx_thread_sleep(5000);
    // }
}