#include "commands/inc/command0128_2.h"

#include <stdio.h>
#include <string.h>
#include <stdlib.h>

#include "app_log.h"
#include "crypto_utils.h"
#include "hichain_json.h"
#include "hichain_utils.h"
#include "random_utils.h"

// 宏定义常量
#define GCM_NONCE_LEN 12
#define GCM_TAG_LEN   16
#define SHA256_LEN    32

extern void send_tlv_and_backup(AuthContext_t *ctx, const uint8_t *data, uint16_t len);

static int send_hichain_07(AuthContext_t *ctx) {
    int ret = -1;
    uint8_t *salt = NULL;
    uint8_t *encResult = NULL;
    uint8_t *tlv = NULL;

    printf("Request operationCode: %d - step: %d\n", ctx->hichain_context.operationCode, ctx->hichain_context.step);

    RequestConfig config = {0};
    hichain_RequestConfig_Init(&config, ctx->hichain_context.requestId, ctx->hichain_context.operationCode);

    // --- 1. 计算 Session Key (保留原逻辑) ---
    size_t salt_len = sizeof(ctx->hichain_context.randSelf) + sizeof(ctx->hichain_context.randPeer);
    salt = (uint8_t *) malloc(salt_len);
    if (!salt) {
        printf("[0128] Error: malloc salt failed\n");
        return -1;
    }

    // [修复 1] 指针偏移，防止覆盖
    uint8_t *p = salt;
    memcpy(p, ctx->hichain_context.randSelf, sizeof(ctx->hichain_context.randSelf));
    p += sizeof(ctx->hichain_context.randSelf);
    memcpy(p, ctx->hichain_context.randPeer, sizeof(ctx->hichain_context.randPeer));

    static uint8_t info[] = "hichain_iso_session_key";

    // 生成 SessionKey
    crypto_hkdf_sha256(ctx->hichain_context.psk, sizeof(ctx->hichain_context.psk),
                       salt, salt_len,
                       info, sizeof(info) - 1,
                       ctx->hichain_context.sessionKey, sizeof(ctx->hichain_context.sessionKey));

    print_hex("[0128] SessionKey", ctx->hichain_context.sessionKey, sizeof(ctx->hichain_context.sessionKey));

    // --- 2. 准备加密数据 ---
    uint8_t nonce[GCM_NONCE_LEN];
    Random_GetByteArray(nonce, GCM_NONCE_LEN);

    static uint8_t step_7_input[] = {0, 0, 0, 0};
    static uint8_t step_7_add[] = "hichain_iso_result";
    // [修复 2] AAD 长度通常不包含结束符 '\0'
    size_t aad_len = sizeof(step_7_add) - 1;

    // [修复 3] 输出缓冲区大小必须包含 AES-GCM Tag (16字节)
    // 如果 input 是 4 字节，output 至少要 20 字节
    size_t encResultBufLen = sizeof(step_7_input) + GCM_TAG_LEN;
    encResult = (uint8_t*)malloc(encResultBufLen);
    if (!encResult) {
        goto cleanup; // 使用 goto 统一释放 salt
    }

    size_t encResultOutLen = encResultBufLen;

    psa_status_t status = crypto_aes_gcm_encrypt(ctx->hichain_context.sessionKey, sizeof(ctx->hichain_context.sessionKey),
                                        nonce, sizeof(nonce),
                                        step_7_add, aad_len,
                                        step_7_input, sizeof(step_7_input),
                                        encResult, encResultBufLen, &encResultOutLen);

    if (status != SUCCESS) {
        printf("[0128] Encrypt failed: %d\n", status);
        goto cleanup;
    }

    size_t tlv_len = 0;

    // [保留用户逻辑] 继续使用 create_step_four 且参数为 3
    tlv = create_step_four(&config, 3, nonce, GCM_NONCE_LEN, encResult, encResultOutLen, &tlv_len);

    if (tlv) {
        send_tlv_and_backup(ctx, tlv, tlv_len);
        ret = 0;
    }

cleanup:
    // [修复 4] 统一释放所有资源，防止 salt 内存泄漏
    if (salt) free(salt);
    if (encResult) free(encResult);
    if (tlv) free(tlv);
    return ret;
}

int Handle0128_6(AuthContext_t *ctx, uint8_t *data, int len) {
    int ret = -1;
    static uint8_t input_data[] = {0x00, 0x00, 0x00, 0x00};

    ResponseData *resp = parse_response(data, len);
    if (resp == NULL) {
        printf("[0128] resp is null\n");
        return -1;
    }

    // [保留用户逻辑] 使用 step2 获取 Step 6 的数据
    uint8_t *returnCode = resp->data.step2.return_code_mac;
    size_t returnCodeLen = resp->data.step2.return_code_mac_len;

    // 长度检查
    if (returnCodeLen != SHA256_LEN) {
         printf("[0128] Invalid MAC len: %d\n", returnCodeLen);
         free_response_data(resp);
         return -1;
    }

    // [修复 5] 增加 malloc 失败检查
    uint8_t *returnCodeCheck = (uint8_t *) malloc(SHA256_LEN);
    if (returnCodeCheck == NULL) {
        free_response_data(resp);
        return -1;
    }

    size_t returnCodeLenCheck = SHA256_LEN;
    crypto_hmac_sha256(ctx->hichain_context.psk, sizeof(ctx->hichain_context.psk),
                       input_data, sizeof(input_data),
                       returnCodeCheck, SHA256_LEN, &returnCodeLenCheck);

    if (memcmp(returnCode, returnCodeCheck, SHA256_LEN) != 0) {
        printf("returnCode check failed\n");
        print_hex("Recv", returnCode, returnCodeLen);
        print_hex("Calc", returnCodeCheck, returnCodeLenCheck);

        free(returnCodeCheck);
        free_response_data(resp);
        return -1;
    }

    printf("returnCode check ok\n");

    // 更新状态
    ctx->hichain_context.step = 7;
    ctx->current_retry_times = ctx->retry_times;
    ctx->state = AUTH_STATE_WAIT_0128;
    ctx->hichain_context.operationCode = 2;

    if (send_hichain_07(ctx) == 0) {
        ret = 0;
    } else {
        ret = -1;
    }

    free(returnCodeCheck);
    free_response_data(resp);
    return ret;
}