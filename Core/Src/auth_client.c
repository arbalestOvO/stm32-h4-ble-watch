//
// Created by 19571 on 2025/12/26.
//

#include "auth_client.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "tx_api.h"
#include "commands/inc/command0101.h"


#define LOG_INFO(fmt, ...)  printf("[INFO] AUTH " fmt "\n", ##__VA_ARGS__)
#define LOG_WARN(fmt, ...)  printf("[WARN] AUTH " fmt "\n", ##__VA_ARGS__)
#define LOG_ERR(fmt, ...)   printf("[ERROR] AUTH " fmt "\n", ##__VA_ARGS__)

static TX_QUEUE tx_queue;
static uint8_t queue_buf[1024];

static const ProtocolEntry_t g_protocol_table[] = {
    {AUTH_STATE_WAIT_0101, 0x0101, Handle0101}
};
#define TABLE_SIZE (sizeof(g_protocol_table) / sizeof(ProtocolEntry_t))

int send_tlv(uint8_t* data, int len);
void send_tlv_and_backup(AuthContext_t* ctx, uint8_t* data, int len);

void resend_last_packet(AuthContext_t* ctx) {
    if (ctx->last_len > 0) {
        LOG_WARN("Resending last packet (len=%d)...", ctx->last_len);
        send_tlv(ctx->last_buf, ctx->last_len);
    }
}

AuthContext_t * AuthContext_Create(char* mac, int timeout_ms, int retryTimes) {
    AuthContext_t* authContext = (AuthContext_t*)malloc(sizeof(AuthContext_t));
    authContext->current_retry_times = retryTimes;
    authContext->timeout_ms = timeout_ms;
    authContext->retry_times = retryTimes;
    *(char**)(&(authContext->mac)) = mac;
    if (tx_queue_create(&tx_queue, "spi to auth thread", 1, queue_buf, 1024) != TX_SUCCESS) {
        LOG_ERR("Failed to create TX queue");
        free(authContext);
        return NULL;
    }
    return authContext;
}

AuthResult_t auth(AuthContext_t *context) {
    if (!context) return AUTH_STATUS_ERROR;
    LOG_INFO("auth start");
    MsgEvent_t msg;
    uint8_t link_tlv[] = {0x01, 0x01, 0x01, 0x00, 0x02, 0x00, 0x03, 0x00, 0x04, 0x00};
    context->state = AUTH_STATE_WAIT_0101;
    send_tlv_and_backup(context, link_tlv, sizeof(link_tlv));
    while (1) {
        if (context->state == AUTH_STATE_AUTHENTICATED) {
            return AUTH_STATUS_OK;
        }
        if (context->state == AUTH_STATE_FAILED) {
            return AUTH_STATUS_ERROR;
        }
        int status = tx_queue_receive(&tx_queue, &msg, context->timeout_ms);
        if (status == TX_SUCCESS) {
            uint8_t found = 0;
            for (int i = 0; i < TABLE_SIZE; i++) {
                if (g_protocol_table[i].cmd_id == msg.id) {
                    if (context->state == g_protocol_table[i].required_state) {
                        LOG_INFO("Handling ID 0x%04X...", msg.id);

                        // 执行处理，如果处理成功，状态机会流转，且在该函数内
                        // 会调用 send_tlv_and_backup 重置 retry 计数器
                        g_protocol_table[i].handler(context, msg.payload, msg.len);
                        found = 1;
                        break;
                    }
                }
            }
            if (!found) LOG_INFO("Ignored ID: 0x%04X", msg.id);
            if (msg.payload) free(msg.payload);
        } else if (status == TX_QUEUE_EMPTY) {
            LOG_WARN("Timeout waiting for response in state %d", context->state);

            if (context->current_retry_times > 0) {
                context->current_retry_times--;
                LOG_WARN("Retrying... (%d attempts left)", context->current_retry_times);
                resend_last_packet(context);
            } else {
                LOG_ERR("Max retries exhausted. Auth Failed.");
                context->state = AUTH_STATE_FAILED;
                return AUTH_STATUS_ERROR;
            }
            continue;
        } else {
            LOG_ERR("Failed to receive");
            return AUTH_STATUS_ERROR;
        }
    }
    return AUTH_STATUS_OK;
}

void AuthContext_Free(AuthContext_t *context) {
    free(context);
    context = NULL;
    tx_queue_delete(&tx_queue);
}

void on_tlv_received(uint8_t* data, int len) {
    if (data == NULL || len < 2) return;

    uint16_t id = (uint8_t)data[0] << 8 | (uint8_t)data[1];
    uint8_t* payload_copy = (uint8_t*)malloc(len - 2);
    if (payload_copy && len > 2) {
        memcpy(payload_copy, &data[2], len - 2);
    }

    MsgEvent_t msg;
    msg.id = id;
    msg.len = len - 2;
    msg.payload = payload_copy;

    // tx_queue_send(&g_auth_ctx.msg_queue, &msg, TX_NO_WAIT);
    LOG_INFO("Pushing Msg ID: 0x%04X to queue", id);
}


int send_tlv(uint8_t* data, int len) {
    printf("[NETWORK] Sending %d bytes: ID 0x%02X%02X\n", len, data[0], data[1]);
    return 0;
}

uint8_t cp_buf[8 * 1024];

void send_tlv_and_backup(AuthContext_t* ctx, uint8_t* data, int len) {
    if (len > sizeof(cp_buf)) {
        LOG_ERR("Packet too large to backup!");
        return;
    }

    // 1. 备份数据，用于重试
    memcpy(cp_buf, data, len);
    ctx->last_len = len;
    ctx->last_buf = *(uint8_t**)&cp_buf;

    // 2. 复位重试计数器 (每次发送新指令时重置)
    ctx->current_retry_times = ctx->retry_times;

    // 3. 真正发送
    send_tlv(data, len);
}