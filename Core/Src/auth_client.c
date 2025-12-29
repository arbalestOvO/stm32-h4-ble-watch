//
// Created by 19571 on 2025/12/26.
//

#include "auth_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ble_client.h"
#include "protocol_5a_parse.h"
#include "tx_api.h"
#include "commands/inc/command0101.h"
#include "commands/inc/command012c.h"
#include "commands/inc/command0128.h"
#include "commands/inc/command0107.h"
#include "commands/inc/command0105.h"
#include "commands/inc/command0102.h"
#include "commands/inc/command0103.h"
#include "commands/inc/command0137.h"
#include "commands/inc/command1a05.h"
#include "commands/inc/command0131.h"
#include "commands/inc/command0130.h"
#include "commands/inc/command013f.h"
#include "commands/inc/command013e.h"
#include "commands/inc/command0135.h"

#define LOG_INFO(fmt, ...)  printf("[INFO] AUTH " fmt "\n", ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  printf("[WARN] AUTH " fmt "\n", ##__VA_ARGS__)
#define LOG_ERR(fmt, ...)   printf("[ERROR] AUTH " fmt "\n", ##__VA_ARGS__)


#define QUEUE_SIZE_BYTES    2048
#define MSG_QUEUE_SIZE      10  // 队列深度，消息个数
#define MAX_RETRY_BUFFER    (1024 * 8) // 限制最大备份包大小，防止内存耗尽
#define MAX_TLV_BUFFER    (1024 * 8) // 限制最大TLV大小，防止内存耗尽

static const ProtocolEntry_t g_protocol_table[] = {
    {AUTH_STATE_WAIT_0101, 0x0101, Handle0101},
{AUTH_STATE_WAIT_0133, 0x0101, Handle0101},
{AUTH_STATE_WAIT_012C, 0x012C, Handle012c},
{AUTH_STATE_WAIT_0128, 0x0128, Handle0128},
{AUTH_STATE_WAIT_0107, 0x0107, Handle0107},
{AUTH_STATE_WAIT_0105, 0x0105, Handle0105},
{AUTH_STATE_WAIT_0102, 0x0102, Handle0102},
{AUTH_STATE_WAIT_0103, 0x0103, Handle0103},
{AUTH_STATE_WAIT_0137, 0x0137, Handle0137},
{AUTH_STATE_WAIT_1A05, 0x1A05, Handle1a05},
{AUTH_STATE_WAIT_0131, 0x0131, Handle0131},
{AUTH_STATE_WAIT_0130, 0x0130, Handle0130},
{AUTH_STATE_WAIT_013F, 0x013f, Handle013f},
{AUTH_STATE_WAIT_013E, 0x013e, Handle013e},
{AUTH_STATE_WAIT_0135, 0x0135, Handle0135},
};

#define TABLE_SIZE (sizeof(g_protocol_table) / sizeof(ProtocolEntry_t))

// 全局指针，用于连接 BLE 回调和当前上下文 (单例模式妥协)
static AuthContext_t *g_active_ctx = NULL;

static TX_QUEUE tx_queue;
static uint8_t queue_buf[1024];

static char g_uuid[33] = "7410142703F4FDF544C6EE8A7DD3AC29";

void send_tlv_and_backup(AuthContext_t* ctx, const uint8_t* data, uint16_t len);
static void cleanup_queue_messages(AuthContext_t* ctx);

void resend_last_packet(AuthContext_t* ctx);

AuthContext_t * AuthContext_Create(char* mac, int timeout_ms, int retryTimes) {
    AuthContext_t* ctx = (AuthContext_t*)malloc(sizeof(AuthContext_t));
    if (!ctx) {
        LOG_ERR("Malloc context failed");
        return NULL;
    }
    memset(ctx, 0, sizeof(AuthContext_t));

    ctx->current_retry_times = retryTimes;
    ctx->timeout_ms = timeout_ms;
    ctx->retry_times = retryTimes;
    ctx->mtu = 20;
    ctx->mfs = 20;
    strcpy(ctx->uuid, g_uuid);
    if (mac) {
        strncpy(ctx->mac, mac, sizeof(ctx->mac) - 1);
    }
    ctx->queue_mem = malloc(QUEUE_SIZE_BYTES);
    if (!ctx->queue_mem) {
        LOG_ERR("Malloc queue memory failed");
        free(ctx);
        return NULL;
    }
    UINT status = tx_queue_create(&ctx->tx_queue, "auth_queue",
                                  sizeof(MsgEvent_t) / sizeof(ULONG),
                                  ctx->queue_mem, QUEUE_SIZE_BYTES);

    if (status != TX_SUCCESS) {
        LOG_ERR("Create queue failed: %d", status);
        free(ctx->queue_mem);
        free(ctx);
        return NULL;
    }
    ctx->last_buf = NULL;
    ctx->last_len = 0;
    return ctx;
}

void AuthContext_Free(AuthContext_t *context) {
    if (!context) return;
    if (g_active_ctx == context) {
        g_active_ctx = NULL;
    }
    cleanup_queue_messages(context);
    tx_queue_delete(&context->tx_queue);
    if (context->queue_mem) free(context->queue_mem);
    if (context->last_buf) free(context->last_buf);
    free(context);
}

static void clear_assembly_buffer(AuthContext_t* ctx) {
    if (ctx->temp_asm_buf) {
        free(ctx->temp_asm_buf);
        ctx->temp_asm_buf = NULL;
    }
    ctx->temp_asm_len = 0;
    ctx->next_fsn = 0;
}

void on_received_frame(uint8_t control, uint8_t fsn, uint8_t *data, uint16_t len) {
    if (g_active_ctx == NULL) return;
    AuthContext_t* ctx = g_active_ctx;

    // ---------------------------------------------------------
    // Case 0: 不分帧 (Unfragmented)
    // ---------------------------------------------------------
    if (control == 0) {
        // 如果之前有烂尾的组包任务，直接丢弃，避免内存泄露或状态错乱
        if (ctx->temp_asm_buf != NULL) {
            printf("[WARN] Dropping incomplete packet due to Control 0\n");
            clear_assembly_buffer(ctx);
        }
        // 直接透传处理，不分配额外堆内存
        on_tlv_received(data, len);
        return;
    }

    // ---------------------------------------------------------
    // Case 1: 起始帧 (Start)
    // ---------------------------------------------------------
    if (control == 1) {
        // 收到 Start，意味着必须开始新任务，先清理旧的（如果有）
        clear_assembly_buffer(ctx);

        if (len == 0) return; // 只有头没有体的情况防御

        ctx->temp_asm_buf = (uint8_t*)malloc(len);
        if (!ctx->temp_asm_buf) {
            printf("[ERR] OOM: Start frame\n");
            return;
        }

        memcpy(ctx->temp_asm_buf, data, len);
        ctx->temp_asm_len = len;
        ctx->next_fsn = (fsn + 1) & 0xFF; // 记录期望的下一帧
        return;
    }

    // ---------------------------------------------------------
    // Case 2 & 3: 中间帧 (Middle) / 末尾帧 (End)
    // ---------------------------------------------------------
    if (control == 2 || control == 3) {
        // 1. 校验前置状态：如果没有 Buffer，说明没收到 Start，丢弃
        if (ctx->temp_asm_buf == NULL) {
            printf("[WARN] Dropping orphan frame (ctrl=%d, fsn=%d)\n", control, fsn);
            return;
        }

        // 2. 校验序号：如果不连续，说明丢包了，整个包作废
        if (fsn != ctx->next_fsn) {
            printf("[ERR] FSN mismatch: exp %d, got %d. Resetting.\n", ctx->next_fsn, fsn);
            clear_assembly_buffer(ctx);
            return;
        }

        // 3. 扩容 (Realloc)
        uint32_t new_total_len = ctx->temp_asm_len + len;
        uint8_t* new_ptr = (uint8_t*)realloc(ctx->temp_asm_buf, new_total_len);

        if (!new_ptr) {
            printf("[ERR] OOM: Realloc failed\n");
            clear_assembly_buffer(ctx); // realloc 失败不会释放原内存，需手动释放
            return;
        }

        ctx->temp_asm_buf = new_ptr; // 更新指针 (realloc 可能移动地址)

        // 4. 追加数据
        if (len > 0) {
            memcpy(ctx->temp_asm_buf + ctx->temp_asm_len, data, len);
            ctx->temp_asm_len += len;
        }

        ctx->next_fsn = (fsn + 1) & 0xFF; // 更新期望序号

        // 5. 如果是末尾帧，提交并销毁
        if (control == 3) {
            // ---> 关键点：调用回调处理完整包 <---
            on_tlv_received(ctx->temp_asm_buf, ctx->temp_asm_len);

            // ---> 处理完立即释放内存，不留在 ctx 中 <---
            clear_assembly_buffer(ctx);
        }
    }
}

/**
 * @brief 清理队列残留消息
 */
static void cleanup_queue_messages(AuthContext_t* ctx) {
    MsgEvent_t msg;
    while (tx_queue_receive(&ctx->tx_queue, &msg, TX_NO_WAIT) == TX_SUCCESS) {
        if (msg.payload) {
            free(msg.payload);
        }
    }
}

void on_tlv_received(uint8_t* data, int len) {
    if (data == NULL || len < 2) return;
    if (g_active_ctx == NULL) {
        // LOG_WARN("No active auth context, dropping packet");
        return;
    }

    // 解析 ID
    uint16_t id = ((uint16_t)data[0] << 8) | data[1];

    // 拷贝 Payload
    uint8_t* payload_copy = NULL;
    uint16_t payload_len = len - 2;

    if (payload_len > 0) {
        payload_copy = (uint8_t*)malloc(payload_len);
        if (!payload_copy) {
            LOG_ERR("OOM handling 0x%04X", id);
            return;
        }
        memcpy(payload_copy, &data[2], payload_len);
    }

    MsgEvent_t msg;
    msg.id = id;
    msg.len = payload_len;
    msg.payload = payload_copy; // 传递指针所有权

    // 发送到队列
    UINT status = tx_queue_send(&g_active_ctx->tx_queue, &msg, TX_NO_WAIT);
    if (status != TX_SUCCESS) {
        LOG_ERR("Queue full, drop 0x%04X", id);
        if (payload_copy) free(payload_copy); // 发送失败需释放，否则泄露
    } else {
        LOG_INFO("Recv ID: 0x%04X, Pushed to queue", id);
    }
}



AuthResult_t auth(AuthContext_t *context) {
    if (!context) return AUTH_STATUS_ERROR;
    g_active_ctx = context;
    LOG_INFO("Auth start for MAC: %s", context->mac);
    MsgEvent_t msg;
    static const uint8_t link_tlv[] = {0x01, 0x01, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x04, 0x00};
    context->state = AUTH_STATE_WAIT_0101;
    send_tlv_and_backup(context, link_tlv, sizeof(link_tlv));
    AuthResult_t result = AUTH_STATUS_ERROR;
    while (1) {
        // 状态检查
        if (context->state == AUTH_STATE_AUTHENTICATED) {
            result = AUTH_STATUS_OK;
            break;
        }
        if (context->state == AUTH_STATE_FAILED) {
            result = AUTH_STATUS_ERROR;
            break;
        }

        // 接收消息
        UINT status = tx_queue_receive(&context->tx_queue, &msg, context->timeout_ms);

        if (status == TX_SUCCESS) {
            uint8_t handled = 0;
            // 遍历协议表
            for (int i = 0; i < TABLE_SIZE; i++) {
                if (g_protocol_table[i].cmd_id == msg.id) {
                    if (context->state == g_protocol_table[i].required_state) {
                        LOG_INFO("Handling ID 0x%04X...", msg.id);
                        // Handler 内部负责状态流转
                        g_protocol_table[i].handler(context, msg.payload, msg.len);
                        handled = 1;
                        break;
                    } else {
                        LOG_WARN("ID 0x%04X received but state mismatch (curr: %d, req: %d)",
                                 msg.id, context->state, g_protocol_table[i].required_state);
                    }
                }
            }
            if (!handled) {
                LOG_INFO("Ignored unknown ID: 0x%04X", msg.id);
            }
            // 务必释放接收到的 payload
            if (msg.payload) free(msg.payload);

        } else if (status == TX_QUEUE_EMPTY) { // ThreadX 超时返回 TX_QUEUE_EMPTY (需确认头文件定义，通常非0)

            LOG_WARN("Timeout waiting response in state %d", context->state);

            if (context->current_retry_times > 0) {
                context->current_retry_times--;
                LOG_WARN("Retrying... (%d attempts left)", context->current_retry_times);
                resend_last_packet(context);
            } else {
                LOG_ERR("Max retries exhausted. Auth Failed.");
                context->state = AUTH_STATE_FAILED;
                result = AUTH_STATUS_ERROR;
                break;
            }
        } else {
            LOG_ERR("Queue receive error: %d", status);
            result = AUTH_STATUS_ERROR;
            break;
        }
    }
    // 鉴权结束，清除激活状态
    g_active_ctx = NULL;
    // 清理可能残留的消息
    cleanup_queue_messages(context);

    return result;
}

void send_tlv(uint8_t* data, uint16_t len) {
    if (!g_active_ctx) return;

    uint16_t current_mtu = g_active_ctx->mtu;
    if (current_mtu == 0) current_mtu = 20; // 保护

    int current_idx = 0;
    const uint8_t* p_data = data;

    // 简单的分包逻辑
    while (current_idx < len) {
        uint16_t packet_len = (len - current_idx > current_mtu) ? current_mtu : (len - current_idx);

        // 注意：0x1B Handle 最好也是传入参数或存在 ctx 中
        android_ble_write_char(0x1B, p_data, packet_len, 1);

        current_idx += packet_len;
        p_data += packet_len;

        // 仅在分包时延时，或根据 BLE 栈的流控移除此延时
        if (current_idx < len) {
            tx_thread_sleep(10); // 减小延时，30ms 太长
        }
    }
}

void send_tlv_and_backup(AuthContext_t* ctx, const uint8_t* data, uint16_t len) {
    if (len > MAX_RETRY_BUFFER) {
        LOG_ERR("Packet too large to backup (%d)", len);
        return;
    }
    if (ctx->last_buf) {
        free(ctx->last_buf);
        ctx->last_buf = NULL;
    }
    ctx->last_buf = (uint8_t*)malloc(len);
    if (ctx->last_buf) {
        memcpy(ctx->last_buf, data, len);
        ctx->last_len = len;
    } else {
        LOG_ERR("Backup malloc failed");
        ctx->last_len = 0;
    }
    ctx->current_retry_times = ctx->retry_times;

    printf("[BLUETOOTH] Sending %d bytes: ID 0x%02X%02X\n", len, data[0], data[1]);
    printf("[SEND TLV]: ");
    for (int i = 0; i < len; i++) {
        printf("%02X", data[i]);
    }
    printf("\n");
    // 3. 真正发送
    parser_send_packet(data, len, ctx->mfs, send_tlv);
}

void resend_last_packet(AuthContext_t* ctx)
{
    if (ctx->last_len > 0 && ctx->last_buf != NULL) {
        LOG_WARN("Resending last packet (len=%d)...", ctx->last_len);
        parser_send_packet(ctx->last_buf, ctx->last_len, ctx->mfs, send_tlv);
    }
}