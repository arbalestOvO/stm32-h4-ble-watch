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

static const char TAG[] = "BT_TEST";

/* HCI 命令结构：OpCode(2bytes) + ParamLen(1byte) + Params */
/* HCI Reset Command: OGF=0x03, OCF=0x0003 -> OpCode = 0x0C03 */
static const uint8_t hci_reset_cmd[] = { 0x01, 0x03, 0x0C, 0x00 };

/* 外部声明 */
extern int esp_hosted_tx(uint8_t iface_type, uint8_t iface_num,
        uint8_t *payload_buf, uint16_t payload_len, uint8_t buff_zcopy,
        uint8_t *buffer_to_free, void (*free_buf_func)(void *ptr), uint8_t flags);

/* 简单的内存释放回调 */
static void my_free_func(void *ptr) {
    if (ptr) {
        free(ptr);
    }
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

    while (1) {
        bt_test_send_reset();

        /* 每 5 秒发一次，方便示波器抓波形 */
        tx_thread_sleep(5000);
    }
}