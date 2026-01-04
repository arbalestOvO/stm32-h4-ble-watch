#include "commands/inc/command0128_5.h"

#include <stdio.h>
#include <string.h>

#include "app_log.h"
#include "crypto_utils.h"
#include "hichain_json.h"
#include "hichain_utils.h"
#include "huawei_tlv.h"

#define SHA256_LEN 32
#define PIN_CODE_HEX_BUF_LEN 128
#define MAX_AUTH_ID_LEN sizeof(((AuthContext_t*)0)->hichain_context.authIdPeer)
#define MAX_SALT_LEN sizeof(((AuthContext_t*)0)->hichain_context.randPeer)

// --- 辅助函数声明 ---
static int update_peer_info(AuthContext_t *ctx, ResponseData *resp);
static int derive_psk(AuthContext_t *ctx);
static int verify_and_generate_token(AuthContext_t *ctx, const uint8_t *peer_token, uint8_t *out_calc_token);

extern void send_tlv_and_backup(AuthContext_t *ctx, const uint8_t *data, uint16_t len);


static void send_hichain_06(AuthContext_t* ctx, uint8_t *selfToken) {
    printf("Request operationCode: %d - step: %d\n", ctx->hichain_context.operationCode, ctx->hichain_context.step);
    RequestConfig config;
    hichain_RequestConfig_Init(&config, ctx->hichain_context.requestId, ctx->hichain_context.operationCode);
    size_t tlv_len;
    uint8_t* tlv = create_step_two(&config, 2, selfToken, 32, &tlv_len);
    send_tlv_and_backup(ctx, tlv, tlv_len);
    free(tlv);
    free(selfToken);
}


int Handle0128_5(AuthContext_t *ctx, uint8_t *data, int len) {
    int ret = -1;
    ResponseData *resp = parse_response(data, len);

    if (resp == NULL) {
        printf("[0128] Error: resp is null\n");
        return -1;
    }

    // 1. 提取并更新对端信息 (AuthID, Salt)
    if (update_peer_info(ctx, resp) != 0) {
        goto cleanup;
    }

    // 2. 根据 PIN 码派生 PSK
    if (derive_psk(ctx) != 0) {
        goto cleanup;
    }

    // 3. 校验 Token 并获取计算出的 Token 用于下一步
    uint8_t *calc_token = (uint8_t *)malloc(SHA256_LEN);
    if (calc_token == NULL) {
        printf("[0128] Error: OOM for token\n");
        goto cleanup;
    }

    if (verify_and_generate_token(ctx, resp->data.step1.token, calc_token) != 0) {
        free(calc_token);
        goto cleanup;
    }

    // 4. 校验通过，更新状态并发送下一步
    printf("token check pass\n");
    ctx->hichain_context.step = 6;
    ctx->current_retry_times = ctx->retry_times;
    ctx->state = AUTH_STATE_WAIT_0128;
    ctx->hichain_context.operationCode = 2;

    // 注意：send_hichain_02 负责发送，我们负责释放 calc_token
    send_hichain_06(ctx, calc_token);

    free(calc_token);
    ret = 0;

cleanup:
    free_response_data(resp);
    return ret;
}

// ==========================================
// 辅助函数实现
// ==========================================

/**
 * @brief 步骤1: 从响应中提取 Peer AuthID 和 Salt 并存入 Context
 */
static int update_peer_info(AuthContext_t *ctx, ResponseData *resp) {
    // 安全检查：防止缓冲区溢出
    if (resp->data.step1.peer_auth_id_len > MAX_AUTH_ID_LEN ||
        resp->data.step1.iso_salt_len > MAX_SALT_LEN) {
        printf("[0128] Error: Peer data overflow. ID: %d, Salt: %d\n",
               resp->data.step1.peer_auth_id_len, resp->data.step1.iso_salt_len);
        return -1;
    }

    memcpy(ctx->hichain_context.authIdPeer, resp->data.step1.peer_auth_id, resp->data.step1.peer_auth_id_len);
    memcpy(ctx->hichain_context.randPeer, resp->data.step1.iso_salt, resp->data.step1.iso_salt_len);
    return 0;
}

/**
 * @brief 步骤2: PinCode -> Hex -> SHA256(Key) -> HMAC(PSK)
 */
static int derive_psk(AuthContext_t *ctx) {
    // 转换 PIN 码为 Hex 字符串
    uint8_t *key = ctx->secretKey;
    size_t key_len = sizeof(ctx->secretKey);
    print_hex("0128 Key", key, key_len);

    // 计算 PSK
    size_t psk_len = 32;
    crypto_hmac_sha256(key, key_len, ctx->hichain_context.seed, 32,
                       ctx->hichain_context.psk, sizeof(ctx->hichain_context.psk), &psk_len);

    print_hex("0128 PSK", ctx->hichain_context.psk, psk_len);
    return 0;
}

/**
 * @brief 步骤3: 构建 Message，计算 Token 并与 Peer Token 比对
 * @param out_calc_token 用于输出计算结果，供下一步发送使用
 */
static int verify_and_generate_token(AuthContext_t *ctx, const uint8_t *peer_token, uint8_t *out_calc_token) {
    // 1. 准备 Message 缓冲区
    size_t message_size = sizeof(ctx->hichain_context.randPeer) + sizeof(ctx->hichain_context.randSelf) +
                          sizeof(ctx->hichain_context.authIdSelf) + sizeof(ctx->hichain_context.authIdPeer);

    uint8_t *message = (uint8_t *)malloc(message_size);
    if (message == NULL) return -1;

    // [DEBUG] 打印拼接前的各个分量，用于排查数据源是否正确
    printf("[0128] --- Token Generation Components ---\n");
    print_hex("  1. Rand Peer", ctx->hichain_context.randPeer, sizeof(ctx->hichain_context.randPeer));
    print_hex("  2. Rand Self", ctx->hichain_context.randSelf, sizeof(ctx->hichain_context.randSelf));
    print_hex("  3. AuthId Self", ctx->hichain_context.authIdSelf, sizeof(ctx->hichain_context.authIdSelf));
    print_hex("  4. AuthId Peer", ctx->hichain_context.authIdPeer, sizeof(ctx->hichain_context.authIdPeer));

    // 2. 拼接 Message
    uint8_t *p = message;
    memcpy(p, ctx->hichain_context.randPeer, sizeof(ctx->hichain_context.randPeer));
    p += sizeof(ctx->hichain_context.randPeer);
    memcpy(p, ctx->hichain_context.randSelf, sizeof(ctx->hichain_context.randSelf));
    p += sizeof(ctx->hichain_context.randSelf);
    memcpy(p, ctx->hichain_context.authIdSelf, sizeof(ctx->hichain_context.authIdSelf));
    p += sizeof(ctx->hichain_context.authIdSelf);
    memcpy(p, ctx->hichain_context.authIdPeer, sizeof(ctx->hichain_context.authIdPeer));

    // [DEBUG] 打印拼接后的完整 Message (HMAC Input)
    print_hex("[0128] Token HMAC Message Input", message, message_size);
    // [DEBUG] 再次确认使用的 Key (PSK)
    print_hex("[0128] Token HMAC Key (PSK)", ctx->hichain_context.psk, 32);

    // 3. 计算 HMAC (Token)
    size_t token_len = SHA256_LEN;
    crypto_hmac_sha256(ctx->hichain_context.psk, 32, message, message_size, out_calc_token, SHA256_LEN, &token_len);

    // [DEBUG] 打印计算结果和期望结果
    print_hex("[0128] Calculated Token", out_calc_token, token_len);
    print_hex("[0128] Received Peer Token", (uint8_t*)peer_token, token_len);

    // 4. 比对
    if (memcmp(peer_token, out_calc_token, token_len) != 0) {
        printf("[0128] ERROR: Token Check Failed! Mismatch detected.\n");
        free(message);
        return -1;
    }
    p = message;

    memcpy(p, ctx->hichain_context.randSelf, sizeof(ctx->hichain_context.randSelf));
    p += sizeof(ctx->hichain_context.randSelf);

    memcpy(p, ctx->hichain_context.randPeer, sizeof(ctx->hichain_context.randPeer));
    p += sizeof(ctx->hichain_context.randPeer);

    memcpy(p, ctx->hichain_context.authIdPeer, sizeof(ctx->hichain_context.authIdPeer));
    p += sizeof(ctx->hichain_context.authIdPeer);

    memcpy(p, ctx->hichain_context.authIdSelf, sizeof(ctx->hichain_context.authIdSelf));

    printf("[0128] --- Self Token Generation Components ---\n");
    print_hex("[0128] Message Input (Self -> Peer -> IdPeer -> IdSelf)", message, message_size);

    crypto_hmac_sha256(ctx->hichain_context.psk, 32, message, message_size, out_calc_token, SHA256_LEN, &token_len);

    print_hex("[0128] Generated Self Token (To Send)", out_calc_token, token_len);

    // 任务完成，释放缓冲区
    free(message);
    return 0;
}