//
// Created by 19571 on 2025/12/26.
//

#include "auth_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "app_log.h"
#include "ble_client.h"
#include "command0133.h"
#include "command013d.h"
#include "crypto_utils.h"
#include "huawei_tlv.h"
#include "protocol_5a_parse.h"
#include "random_utils.h"
#include "TimeUtils.h"
#include "tx_api.h"
#include "ui_interface.h"
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


extern void on_app_tlv_received(uint16_t id, uint8_t* data, int len);

#define QUEUE_SIZE_BYTES    2048
#define MSG_QUEUE_SIZE      10  // 队列深度，消息个数
#define MAX_RETRY_BUFFER    (1024 * 8) // 限制最大备份包大小，防止内存耗尽
#define MAX_TLV_BUFFER    (1024 * 8) // 限制最大TLV大小，防止内存耗尽
#define AES_IV_LEN 16
#define TAG_LEN 16

static const ProtocolEntry_t g_protocol_table[] = {
    {AUTH_STATE_WAIT_0101, 0x0101, Handle0101},
{AUTH_STATE_WAIT_0133, 0x0133, Handle0133},
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
{AUTH_STATE_WAIT_013D, 0x013d, Handle013d},
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
int decrypt(uint8_t **tlv, size_t *len);

AuthContext_t * AuthContext_Create(char* mac, int timeout_ms, int retryTimes) {
    AuthContext_t* ctx = (AuthContext_t*)malloc(sizeof(AuthContext_t));
    if (!ctx) {
        LOG_ERR("Malloc context failed");
        return NULL;
    }
    memset(ctx, 0, sizeof(AuthContext_t));
    memset(&ctx->hichain_context, 0, sizeof(HiChainContext));
    ctx->hichain_context.requestId = Time_GetUnixTimestamp();
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

    // 只有在显式 Free 时才解绑全局上下文
    if (g_active_ctx == context) {
        g_active_ctx = NULL;
    }

    cleanup_queue_messages(context);
    tx_queue_delete(&context->tx_queue);

    if (context->queue_mem) free(context->queue_mem);
    if (context->last_buf) free(context->last_buf);
    if (context->temp_asm_buf) free(context->temp_asm_buf);

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

    // Case 0: 不分帧 (Unfragmented)
    if (control == 0) {
        if (ctx->temp_asm_buf != NULL) {
            clear_assembly_buffer(ctx);
        }
        on_tlv_received(data, len);
        return;
    }

    // Case 1: 起始帧 (Start)
    if (control == 1) {
        clear_assembly_buffer(ctx);
        if (len == 0) return;

        ctx->temp_asm_buf = (uint8_t*)malloc(len);
        if (!ctx->temp_asm_buf) return;

        memcpy(ctx->temp_asm_buf, data, len);
        ctx->temp_asm_len = len;
        ctx->next_fsn = (fsn + 1) & 0xFF;
        return;
    }

    // Case 2 & 3: 中间帧 / 末尾帧
    if (control == 2 || control == 3) {
        if (ctx->temp_asm_buf == NULL) return;
        if (fsn != ctx->next_fsn) {
            clear_assembly_buffer(ctx);
            return;
        }

        uint32_t new_total_len = ctx->temp_asm_len + len;
        uint8_t* new_ptr = (uint8_t*)realloc(ctx->temp_asm_buf, new_total_len);

        if (!new_ptr) {
            clear_assembly_buffer(ctx);
            return;
        }

        ctx->temp_asm_buf = new_ptr;
        if (len > 0) {
            memcpy(ctx->temp_asm_buf + ctx->temp_asm_len, data, len);
            ctx->temp_asm_len += len;
        }

        ctx->next_fsn = (fsn + 1) & 0xFF;

        if (control == 3) {
            on_tlv_received(ctx->temp_asm_buf, ctx->temp_asm_len);
            clear_assembly_buffer(ctx);
        }
    }
}

static void cleanup_queue_messages(AuthContext_t* ctx) {
    MsgEvent_t msg;
    while (tx_queue_receive(&ctx->tx_queue, &msg, TX_NO_WAIT) == TX_SUCCESS) {
        if (msg.payload) {
            free(msg.payload);
        }
    }
}

int decrypt(uint8_t **tlv, size_t *len) {
    if (!tlv || !*tlv || !len) return -1;
    if (!g_active_ctx) return -1;

    // 注意：确保 secretKey 在结构体中是数组而不是指针，或者是有效指针
    uint8_t* key = g_active_ctx->secretKey;
    size_t key_len = sizeof g_active_ctx->secretKey;

    htlv_view_t view;
    uint8_t iv[AES_IV_LEN];
    if (htlv_find(*tlv, *len, 0x7D, &view) == HTLV_OK) {
        memcpy(iv, view.value, AES_IV_LEN);
    } else {
        return -1;
    }
    uint8_t* payload;
    if (htlv_find(*tlv, *len, 0x7E, &view) == HTLV_OK) {
        payload = (uint8_t*)malloc(view.length);
        if(!payload) return -2;
        memcpy(payload, view.value, view.length);
    } else {
        return -1;
    }
    size_t output_len = (size_t)*len; // 最大可能长度
    size_t expected_output_len = output_len + TAG_LEN;
    uint8_t *temp_encrypted = (uint8_t *)malloc(expected_output_len);
    if (!temp_encrypted) {
        free(payload);
        return -2;
    }

    size_t real_output_len = 0;
    psa_status_t status = crypto_aes_gcm_decrypt(
        key, key_len,
        iv, AES_IV_LEN,
        NULL, 0,
        payload, view.length,
        temp_encrypted, expected_output_len,
        &real_output_len
    );

    free(payload); // payload 是一份拷贝，用完释放

    if (status != 0) {
        free(temp_encrypted);
        return -3;
    }

    uint8_t *new_ptr = (uint8_t *)realloc(*tlv, real_output_len);
    if (!new_ptr && real_output_len > 0) {
        free(temp_encrypted);
        return -2;
    }

    // 如果 realloc 返回新地址或原地址
    *tlv = new_ptr;
    memcpy(*tlv, temp_encrypted, real_output_len);
    *len = real_output_len;

    free(temp_encrypted);
    return 0;
}

void on_tlv_received(uint8_t* data, int len) {
    if (data == NULL || len < 2) return;
    if (g_active_ctx == NULL) return;
    uint16_t id = ((uint16_t)data[0] << 8) | data[1];

    // 拷贝 Payload
    uint8_t* payload_copy = NULL;
    size_t payload_len = len - 2;

    if (payload_len > 0) {
        payload_copy = (uint8_t*)malloc(payload_len);
        if (!payload_copy) {
            LOG_ERR("OOM handling 0x%04X", id);
            return;
        }
        memcpy(payload_copy, &data[2], payload_len);

        // 尝试解密
        if (data[2] == 0x7C) {
            if (decrypt(&payload_copy, &payload_len) != 0) {
                LOG_ERR("Decrypt failed for ID 0x%04X", id);
                free(payload_copy);
                return;
            }
        }
    }

    printf("[REV TLV] ID: 0x%04X, Len: %d\n", id, (int)payload_len);

    // [CRITICAL FIX] 如果已经鉴权成功，直接回调给 Application，不再走 Auth 队列
    if (g_active_ctx->state == AUTH_STATE_AUTHENTICATED) {
        LOG_INFO("App data received (0x%04X), forwarding...", id);
        on_app_tlv_received(id, payload_copy, (int)payload_len);

        // 注意：根据 on_app_tlv_received 的约定，如果它不负责释放，这里需要释放
        // 通常回调模式下，数据在回调返回后失效。这里假设 on_app_tlv_received 拷贝了数据或立即处理。
        if (payload_copy) free(payload_copy);
        return;
    }

    // 鉴权过程中，数据推入队列
    MsgEvent_t msg;
    msg.id = id;
    msg.len = (size_t)payload_len;
    msg.payload = payload_copy; // 传递指针所有权

    UINT status = tx_queue_send(&g_active_ctx->tx_queue, &msg, TX_NO_WAIT);
    if (status != TX_SUCCESS) {
        LOG_ERR("Queue full, drop 0x%04X", id);
        if (payload_copy) free(payload_copy);
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
        if (context->state == AUTH_STATE_AUTHENTICATED) {
            result = AUTH_STATUS_OK;
            LOG_INFO("Auth success for MAC: %s", context->mac);
            break;
        }
        if (context->state == AUTH_STATE_FAILED) {
            result = AUTH_STATUS_ERROR;
            break;
        }

        UINT status = tx_queue_receive(&context->tx_queue, &msg, context->timeout_ms);

        if (status == TX_SUCCESS) {
            uint8_t handled = 0;
            for (int i = 0; i < TABLE_SIZE; i++) {
                if (g_protocol_table[i].cmd_id == msg.id) {
                    if (context->state == g_protocol_table[i].required_state) {
                        g_protocol_table[i].handler(context, msg.payload, msg.len);
                        handled = 1;
                        break;
                    }
                }
            }
            if (!handled) LOG_INFO("Ignored ID: 0x%04X in state %d", msg.id, context->state);
            if (msg.payload) free(msg.payload);

        } else if (status == TX_QUEUE_EMPTY) {
            // 超时重传逻辑
            if (context->current_retry_times > 0) {
                context->current_retry_times--;
                LOG_WARN("Timeout. Retrying... (%d left)", context->current_retry_times);
                resend_last_packet(context);
            } else {
                LOG_ERR("Max retries exhausted.");
                context->state = AUTH_STATE_FAILED;
                result = AUTH_STATUS_ERROR;
                break;
            }
        } else {
            result = AUTH_STATUS_ERROR;
            break;
        }
    }

    // [CRITICAL FIX] 鉴权成功后不要置空 g_active_ctx，
    // 否则 on_received_frame 会丢弃后续的应用数据包。
    // 只在失败时清理，成功时保持连接状态。
    if (result != AUTH_STATUS_OK) {
        ui_show_notify_safe(false, false, "鉴权失败...");
        g_active_ctx = NULL;
        cleanup_queue_messages(context);
    } else {
        // 如果成功，清空队列里剩余的鉴权消息（如果有）
        cleanup_queue_messages(context);
    }

    return result;
}

void send_tlv(uint8_t* data, uint16_t len) {
    if (!g_active_ctx) return;

    uint16_t current_mtu = g_active_ctx->mtu;
    if (current_mtu == 0) current_mtu = 20;

    int current_idx = 0;
    const uint8_t* p_data = data;

    while (current_idx < len) {
        uint16_t packet_len = (len - current_idx > current_mtu) ? current_mtu : (len - current_idx);
        // 根据实际情况调整延时
        tx_thread_sleep(1);
        android_ble_write_char(0x2C, p_data, packet_len, 1);
        current_idx += packet_len;
        p_data += packet_len;
    }
}

int encrypt(uint8_t sid, uint8_t cid, uint8_t **tlv, size_t *len) {
    if (!tlv || !*tlv || !len) return -1;
    if (!g_active_ctx) return -1;

    uint8_t* key = g_active_ctx->secretKey;
    size_t key_len = sizeof g_active_ctx->secretKey;

    uint8_t iv[AES_IV_LEN];
    Random_GetByteArray(iv, AES_IV_LEN);

    size_t input_len = (size_t)*len;
    size_t expected_output_len = input_len + TAG_LEN;
    uint8_t *temp_encrypted = (uint8_t *)malloc(expected_output_len);
    if (!temp_encrypted) return -2;

    size_t real_output_len = 0;
    psa_status_t status = crypto_aes_gcm_encrypt(
        key, key_len,
        iv, AES_IV_LEN,
        NULL, 0,
        *tlv, input_len,
        temp_encrypted, expected_output_len,
        &real_output_len
    );

    if (status != 0) {
        free(temp_encrypted);
        return -3;
    }

    // 计算总长度: SID(1)+CID(1) + Tag7C(1+L) + Tag7D(1+L+IV) + Tag7E(1+L+Data)
    // 这里简单估算 overhead，使用 Huawei TLV 库写入会更准确
    // 假设最大 overhead 100字节足矣
    size_t new_total_len = real_output_len + 128;

    uint8_t *new_ptr = (uint8_t *)realloc(*tlv, new_total_len);
    if (!new_ptr) {
        free(temp_encrypted);
        return -2;
    }
    *tlv = new_ptr;

    htlv_writer_t writer;
    htlv_writer_init(&writer, new_ptr, new_total_len);

    writer.buffer[0] = sid;
    writer.buffer[1] = cid;
    writer.offset = 2;

    // 构造加密后的 TLV 结构
    // 注意: 具体协议结构可能不同，这里参照你代码原意，把加密内容放入 7E
    // Tag 7C (Encrypted Flag?), 7D (IV), 7E (Payload)
    uint8_t data_01 = 0x01;
    htlv_write_tag(&writer, 0x7C, &data_01, 1);
    htlv_write_tag(&writer, 0x7D, iv, AES_IV_LEN);
    htlv_write_tag(&writer, 0x7E, temp_encrypted, real_output_len);

    *len = writer.offset; // 更新为实际封装后的长度
    free(temp_encrypted);
    return 0;
}

void send_tlv_and_backup(AuthContext_t* ctx, const uint8_t* data, uint16_t len) {
    if (len > MAX_RETRY_BUFFER) {
        LOG_ERR("Packet too large to backup");
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
    }
    ctx->current_retry_times = ctx->retry_times;

    printf("[SEND TLV] Len: %d\n", len);
    parser_send_packet(data, len, ctx->mfs, send_tlv);
}

void resend_last_packet(AuthContext_t* ctx) {
    if (ctx->last_len > 0 && ctx->last_buf != NULL) {
        parser_send_packet(ctx->last_buf, ctx->last_len, ctx->mfs, send_tlv);
    }
}

/**
 * @brief 发送应用层数据
 * @param data 包含 SID(byte0) + CID(byte1) + Payload(byte2...) 的完整数据包
 * @param len 数据总长度
 * @param is_encrypt 是否需要加密发送
 */
void send_app_tlv(uint8_t* data, size_t len, bool is_encrypt) {
    // 1. 基础校验
    if (!g_active_ctx || g_active_ctx->state != AUTH_STATE_AUTHENTICATED) {
        LOG_WARN("Cannot send app data: Not authenticated");
        return;
    }

    if (data == NULL || len < 2) {
        LOG_ERR("Invalid data length for app send (must >= 2 for SID/CID)");
        return;
    }

    // 2. 提取 SID 和 CID
    uint8_t sid = data[0];
    uint8_t cid = data[1];

    if (is_encrypt) {
        // --- 加密发送流程 ---

        size_t payload_len = len - 2;

        // 必须分配堆内存，因为 encrypt 函数内部会执行 realloc
        // 如果 payload_len 为 0 (只有头没有体)，malloc(0) 的行为取决于平台，建议处理一下
        uint8_t* tx_buf = NULL;
        if (payload_len > 0) {
            tx_buf = (uint8_t*)malloc(payload_len);
            if (!tx_buf) {
                LOG_ERR("OOM in send_app_tlv");
                return;
            }
            // 复制 Payload 部分 (跳过 SID/CID)
            memcpy(tx_buf, data + 2, payload_len);
        } else {
            // 只有头没有体的情况，encrypt 仍需处理以添加 7C/7D 等空载荷结构
            tx_buf = (uint8_t*)malloc(1); // 分配1字节占位，realloc会调整它
            payload_len = 0;
        }

        // 调用 encrypt
        // encrypt 会做几件事：
        // 1. 自动生成 IV
        // 2. 使用 AES-GCM 加密 tx_buf 内容
        // 3. realloc tx_buf 扩大空间
        // 4. 在头部写入传入的 sid, cid
        // 5. 写入 0x7C, 0x7D(IV), 0x7E(密文)
        int ret = encrypt(sid, cid, &tx_buf, &payload_len);

        if (ret == 0) {
            // 发送加密封装后的数据
            parser_send_packet(tx_buf, payload_len, g_active_ctx->mfs, send_tlv);
        } else {
            LOG_ERR("Encrypt failed: %d", ret);
        }

        // 释放由 malloc 分配 (并可能被 encrypt realloc 过) 的内存
        if (tx_buf) free(tx_buf);

    } else {
        // --- 明文发送流程 ---

        // 直接发送原始数据 (包含 data[0]SID 和 data[1]CID)
        // parser_send_packet 通常会进行分包处理，不会修改原 buffer，所以直接传 data 即可
        parser_send_packet(data, len, g_active_ctx->mfs, send_tlv);
    }
}