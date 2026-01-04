#include "commands/inc/command0128_2.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "app_log.h"
#include "crypto_utils.h"
#include "hichain_json.h"
#include "hichain_utils.h"
#include "random_utils.h"

// 定义常量，避免魔术数字
#define GCM_NONCE_LEN 12
#define GCM_TAG_LEN   16  // AES-GCM Tag 通常为 16 字节
#define SHA256_LEN    32

extern void send_tlv_and_backup(AuthContext_t *ctx, const uint8_t *data, uint16_t len);

// 辅助函数：统一释放资源
static void free_step3_resources(uint8_t *salt, uint8_t *nonce, uint8_t *encData, uint8_t *tlv) {
    if (salt) free(salt);
    if (nonce) free(nonce);
    if (encData) free(encData);
    if (tlv) free(tlv);
}

// 返回 int 以便上层判断是否成功
static int send_hichain_03(AuthContext_t *ctx) {
    int ret = -1;
    uint8_t *salt = NULL;
    uint8_t *nonce = NULL;
    uint8_t *encData = NULL;
    uint8_t *tlv = NULL;

    printf("\n--- [0128] Step 3: Key Derivation & Encryption Start ---\n");
    printf("Request operationCode: %d - step: %d\n", ctx->hichain_context.operationCode, ctx->hichain_context.step);

    RequestConfig config = {0};
    hichain_RequestConfig_Init(&config, ctx->hichain_context.requestId, ctx->hichain_context.operationCode);

    // --- 1. 计算 Session Key (HKDF) ---
    //
    size_t salt_len = sizeof(ctx->hichain_context.randSelf) + sizeof(ctx->hichain_context.randPeer);
    salt = (uint8_t *) malloc(salt_len);
    if (!salt) {
        printf("[0128] Error: malloc salt failed\n");
        goto cleanup;
    }

    // 修复 2: 使用指针偏移，避免覆盖
    uint8_t *p = salt;
    memcpy(p, ctx->hichain_context.randSelf, sizeof(ctx->hichain_context.randSelf));
    p += sizeof(ctx->hichain_context.randSelf);
    memcpy(p, ctx->hichain_context.randPeer, sizeof(ctx->hichain_context.randPeer));

    static uint8_t info[] = "hichain_iso_session_key";

    // [DEBUG] 打印 HKDF 输入参数
    printf("[DEBUG] --- HKDF Parameters ---\n");
    print_hex("[DEBUG] PSK (Input Key)", ctx->hichain_context.psk, sizeof(ctx->hichain_context.psk));
    print_hex("[DEBUG] RandSelf", ctx->hichain_context.randSelf, sizeof(ctx->hichain_context.randSelf));
    print_hex("[DEBUG] RandPeer", ctx->hichain_context.randPeer, sizeof(ctx->hichain_context.randPeer));
    print_hex("[DEBUG] Salt (RandSelf + RandPeer)", salt, salt_len);
    print_hex("[DEBUG] Info", info, sizeof(info) - 1);

    // HKDF 生成 SessionKey
    crypto_hkdf_sha256(ctx->hichain_context.psk, sizeof(ctx->hichain_context.psk),
                       salt, salt_len,
                       info, sizeof(info) - 1,
                       ctx->hichain_context.sessionKey, sizeof(ctx->hichain_context.sessionKey));

    // [DEBUG] 打印生成的 SessionKey (关键检查点)
    print_hex("[DEBUG] >> Generated SessionKey <<", ctx->hichain_context.sessionKey, sizeof(ctx->hichain_context.sessionKey));

    // --- 2. 准备加密数据 (AES-GCM) ---
    nonce = (uint8_t *) malloc(GCM_NONCE_LEN);
    if (!nonce) goto cleanup;

    Random_GetByteArray(nonce, GCM_NONCE_LEN);

    // 生成 Challenge
    Random_GetByteArray(ctx->hichain_context.challenge, sizeof(ctx->hichain_context.challenge));

    // 修复 3: 分配足够的空间 (Plaintext + Tag)
    size_t challenge_len = sizeof(ctx->hichain_context.challenge);
    size_t encDataSize = challenge_len + GCM_TAG_LEN;
    encData = (uint8_t *) malloc(encDataSize);
    if (!encData) goto cleanup;

    static uint8_t aad[] = "hichain_iso_exchange";
    size_t encDataOutLen = encDataSize; // 输入 buffer 大小

    // [DEBUG] 打印加密参数
    printf("[DEBUG] --- GCM Encrypt Parameters ---\n");
    print_hex("[DEBUG] Key", ctx->hichain_context.sessionKey, sizeof(ctx->hichain_context.sessionKey));
    print_hex("[DEBUG] Nonce/IV", nonce, GCM_NONCE_LEN);
    print_hex("[DEBUG] AAD", aad, sizeof(aad) - 1);
    print_hex("[DEBUG] Plaintext (Challenge)", ctx->hichain_context.challenge, challenge_len);

    // 修复 1: 使用 Encrypt 而不是 Decrypt
    psa_status_t status = crypto_aes_gcm_encrypt(
                           ctx->hichain_context.sessionKey, sizeof(ctx->hichain_context.sessionKey),
                           nonce, GCM_NONCE_LEN,
                           aad, sizeof(aad) - 1,
                           ctx->hichain_context.challenge, challenge_len,
                           encData, encDataSize, &encDataOutLen);

    if (status != SUCCESS) {
        printf("[0128] Encrypt failed status: %d\n", status);
        goto cleanup;
    }

    // [DEBUG] 打印加密结果
    print_hex("[DEBUG] EncData (Cipher + Tag)", encData, encDataOutLen);
    printf("--- [0128] Step 3: Encryption End ---\n\n");

    // --- 3. 更新 Context 并发送 ---
    ctx->hichain_context.step = 3;
    ctx->current_retry_times = ctx->retry_times;
    ctx->state = AUTH_STATE_WAIT_0128;
    ctx->hichain_context.operationCode = 1;

    size_t tlv_len = 0;
    // 使用实际加密后的长度 encDataOutLen
    tlv = create_step_three(&config, 3, nonce, GCM_NONCE_LEN, encData, encDataOutLen, &tlv_len);

    if (tlv) {
        // [DEBUG] 打印最终发送的包
        print_hex("[DEBUG] Step3 Final TLV", tlv, tlv_len);
        send_tlv_and_backup(ctx, tlv, tlv_len);
        ret = 0;
    }

cleanup:
    free_step3_resources(salt, nonce, encData, tlv);
    return ret;
}

int Handle0128_2(AuthContext_t *ctx, uint8_t *data, int len) {
    int ret = -1;
    // 假设 input_data 是协议规定的常量
    static uint8_t input_data[] = {0x00, 0x00, 0x00, 0x00};

    printf("\n--- [0128] Handle0128_2: Verify HMAC Start ---\n");
    print_hex("[DEBUG] Received Payload", data, len);

    ResponseData *resp = parse_response(data, len);
    if (resp == NULL) {
        printf("[0128] resp is null\n");
        return -1;
    }

    uint8_t *returnCode = resp->data.step2.return_code_mac;
    size_t returnCodeLen = resp->data.step2.return_code_mac_len;

    // 检查长度是否符合 SHA256
    if (returnCodeLen != SHA256_LEN) {
         printf("[0128] Invalid MAC len: %zu (Expected %d)\n", returnCodeLen, SHA256_LEN);
         free_response_data(resp);
         return -1;
    }

    uint8_t *returnCodeCheck = (uint8_t *) malloc(SHA256_LEN);
    if (returnCodeCheck == NULL) {
        free_response_data(resp);
        return -1;
    }

    size_t returnCodeLenCheck = SHA256_LEN;

    // [DEBUG] 打印 HMAC 计算参数
    print_hex("[DEBUG] HMAC Key (PSK)", ctx->hichain_context.psk, sizeof(ctx->hichain_context.psk));
    print_hex("[DEBUG] HMAC Input Data", input_data, sizeof(input_data));

    crypto_hmac_sha256(ctx->hichain_context.psk, sizeof(ctx->hichain_context.psk),
                       input_data, sizeof(input_data),
                       returnCodeCheck, SHA256_LEN, &returnCodeLenCheck);

    // [DEBUG] 打印对比结果
    printf("[DEBUG] Comparing HMACs:\n");
    print_hex("   Recv", returnCode, returnCodeLen);
    print_hex("   Calc", returnCodeCheck, returnCodeLenCheck);

    if (memcmp(returnCode, returnCodeCheck, SHA256_LEN) != 0) {
        printf("[0128] Error: returnCode check failed (HMAC mismatch)\n");
        free(returnCodeCheck);
        free_response_data(resp);
        return -1;
    }

    printf("[0128] returnCode check OK. Proceeding to Step 3...\n");

    // 修复 4: 检查发送结果
    if (send_hichain_03(ctx) == 0) {
        ret = 0;
    } else {
        printf("[0128] Error: send_hichain_03 failed\n");
        ret = -1;
    }

    free(returnCodeCheck);
    free_response_data(resp);
    return ret;
}