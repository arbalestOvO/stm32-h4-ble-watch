#include "commands/inc/command0128_1.h"

#include <stdio.h>
#include <string.h>

#include "app_log.h"
#include "crypto_utils.h"
#include "hichain_json.h"
#include "hichain_utils.h"
#include "huawei_tlv.h"
#include "stm32h7xx.h"

// --- 常量定义 ---
#define SHA256_LEN 32
#define PIN_CODE_HEX_BUF_LEN 6
#define MAX_AUTH_ID_LEN sizeof(((AuthContext_t*)0)->hichain_context.authIdPeer)
#define MAX_SALT_LEN sizeof(((AuthContext_t*)0)->hichain_context.randPeer)

// --- 辅助函数声明 ---
static int update_peer_info(AuthContext_t *ctx, ResponseData *resp);
static int derive_psk(AuthContext_t *ctx);
static int verify_and_generate_token(AuthContext_t *ctx, const uint8_t *peer_token, uint8_t *out_calc_token);

extern void send_tlv_and_backup(AuthContext_t *ctx, const uint8_t *data, uint16_t len);

static void send_hichain_02(AuthContext_t* ctx, uint8_t *selfToken) {
    printf("Request operationCode: %d - step: %d\n", ctx->hichain_context.operationCode, ctx->hichain_context.step);

    // [DEBUG] 打印最终发送给对方的 Token
    print_hex("[0128] Sending Step2 Token", selfToken, SHA256_LEN);

    RequestConfig config;
    hichain_RequestConfig_Init(&config, ctx->hichain_context.requestId, ctx->hichain_context.operationCode);
    size_t tlv_len;
    uint8_t* tlv = create_step_two(&config, 2, selfToken, SHA256_LEN, &tlv_len);
    send_tlv_and_backup(ctx, tlv, tlv_len);
    free(tlv);
    // 注意：selfToken 在这里被 free 了，Handle0128_1 中不要再次 free，否则会 Double Free
    free(selfToken);
}

int Handle0128_1(AuthContext_t *ctx, uint8_t *data, int len) {
    printf("\n=== [0128] Handle0128_1 Start ===\n");
    int ret = -1;
    ResponseData *resp = parse_response(data, len);

    if (resp == NULL) {
        printf("[0128] Error: resp is null\n");
        return -1;
    }

    // 1. 提取并更新对端信息 (AuthID, Salt)
    printf("[0128] Step 1: Update Peer Info\n");
    if (update_peer_info(ctx, resp) != 0) {
        goto cleanup;
    }

    // 2. 根据 PIN 码派生 PSK
    printf("[0128] Step 2: Derive PSK\n");
    if (derive_psk(ctx) != 0) {
        goto cleanup;
    }

    // 3. 校验 Token 并获取计算出的 Token 用于下一步
    printf("[0128] Step 3: Verify & Generate Token\n");
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
    printf("[0128] Token check PASS. Proceeding to Step 2.\n");
    ctx->hichain_context.step = 2;
    ctx->current_retry_times = ctx->retry_times;
    ctx->state = AUTH_STATE_WAIT_0128;
    ctx->hichain_context.operationCode = 1;

    // 注意：send_hichain_02 内部执行了 free(calc_token)
    send_hichain_02(ctx, calc_token);

    // [FIX] 原代码这里有 free(calc_token)，但 send_hichain_02 里已经 free 了一次。
    // 为了防止 Double Free，这里注释掉，或者修改 send_hichain_02 的行为。
    // 这里假设保持 send_hichain_02 不变，注释掉此处的 free。
    // free(calc_token);

    ret = 0;

cleanup:
    free_response_data(resp);
    printf("=== [0128] Handle0128_1 End (ret=%d) ===\n\n", ret);
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
        printf("[0128] Error: Peer data overflow. ID len: %d, Salt len: %d\n",
               resp->data.step1.peer_auth_id_len, resp->data.step1.iso_salt_len);
        return -1;
    }

    // [DEBUG] 打印接收到的原始数据
    print_hex("[0128] Recv Peer AuthID", resp->data.step1.peer_auth_id, resp->data.step1.peer_auth_id_len);
    print_hex("[0128] Recv Peer Salt", resp->data.step1.iso_salt, resp->data.step1.iso_salt_len);

    memcpy(ctx->hichain_context.authIdPeer, resp->data.step1.peer_auth_id, resp->data.step1.peer_auth_id_len);
    memcpy(ctx->hichain_context.randPeer, resp->data.step1.iso_salt, resp->data.step1.iso_salt_len);

    return 0;
}


/**
 * @brief 步骤2: PinCode -> Hex -> SHA256(Key) -> HMAC(PSK)
 */
static int derive_psk(AuthContext_t *ctx) {
    // [DEBUG] 打印原始 PIN 码 (注意：生产环境可能需要屏蔽)
    print_hex("[0128] Origin PinCode", ctx->pinCode, sizeof ctx->pinCode); // 假设 pinCode 长度足够

    // 转换 PIN 码为 Hex 字符串
    char *pinCodeHexStr = bytes_to_hex(ctx->pinCode, sizeof ctx->pinCode);
    if (pinCodeHexStr == NULL) {
        printf("[0128] Error: Pin hex conversion failed\n");
        return -1;
    }
    printf("[0128] Pin Hex Str: %s\n", pinCodeHexStr);

    uint8_t pinCodeBuf[PIN_CODE_HEX_BUF_LEN];
    memset(pinCodeBuf, 0, sizeof(pinCodeBuf));

    // 安全拷贝
    size_t copy_len = strlen(pinCodeHexStr);
    if (copy_len > sizeof(pinCodeBuf)) copy_len = sizeof(pinCodeBuf);
    memcpy(pinCodeBuf, pinCodeHexStr, copy_len);

    free(pinCodeHexStr);

    // [DEBUG] 打印 SHA256 的输入 (PinCode Hex Buffer)
    // 重要：确认 padding 是否为 0，以及长度是否符合预期
    print_hex("[0128] SHA256 Input (PinBuf)", pinCodeBuf, PIN_CODE_HEX_BUF_LEN);

    // 计算 Key
    uint8_t key[SHA256_LEN];
    size_t key_len = SHA256_LEN;
    psa_status_t status = crypto_sha256(pinCodeBuf, PIN_CODE_HEX_BUF_LEN, key, SHA256_LEN, &key_len);

    if (status != 0) {
        printf("[0128] crypto_sha256 failed: %d\n", status);
        return -1;
    }

    // [DEBUG] 打印生成的 Key (SHA256 Output)
    print_hex("[0128] SHA256 Output (Key)", key, key_len);

    // [DEBUG] 打印 HMAC 的输入 Seed
    print_hex("[0128] HMAC Input (Seed)", ctx->hichain_context.seed, 32);

    // 计算 PSK
    size_t psk_len = 32;
    crypto_hmac_sha256(key, key_len, ctx->hichain_context.seed, 32,
                       ctx->hichain_context.psk, sizeof(ctx->hichain_context.psk), &psk_len);

    // [DEBUG] 打印最终生成的 PSK (Critical!)
    print_hex("[0128] Final PSK", ctx->hichain_context.psk, psk_len);

    return 0;
}

/**
 * @brief 步骤3: 构建 Message，计算 Token 并与 Peer Token 比对
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