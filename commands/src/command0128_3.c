#include "commands/inc/command0128_3.h"

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
#define AUTH_TOKEN_EXPECTED_LEN 32

extern void send_tlv_and_backup(AuthContext_t* ctx, const uint8_t* data, uint16_t len);

static int send_hichain_04(AuthContext_t* ctx) {
    printf("Request operationCode: %d - step: %d\n", ctx->hichain_context.operationCode, ctx->hichain_context.step);

    RequestConfig config = {0};
    hichain_RequestConfig_Init(&config, ctx->hichain_context.requestId, ctx->hichain_context.operationCode);

    uint8_t nonce[GCM_NONCE_LEN];
    Random_GetByteArray(nonce, GCM_NONCE_LEN);

    static uint8_t step_4_input[] = {0, 0, 0, 0}; // 4 bytes result code
    static uint8_t step_4_add[] = "hichain_iso_result";
    size_t aad_len = sizeof(step_4_add) - 1;

    // 计算需要的 buffer 大小: Input Data + Tag
    size_t encResultBufLen = sizeof(step_4_input) + GCM_TAG_LEN;
    uint8_t *encResult = (uint8_t*)malloc(encResultBufLen);
    if (!encResult) return -1;

    size_t encResultOutLen = encResultBufLen;

    // [DEBUG] 打印 Step 4 加密前的关键参数
    printf("\n--- [0128] Step 4 Encryption Start ---\n");
    print_hex("[DEBUG] Step4 SessionKey", ctx->hichain_context.sessionKey, sizeof(ctx->hichain_context.sessionKey));
    print_hex("[DEBUG] Step4 Nonce", nonce, GCM_NONCE_LEN);
    print_hex("[DEBUG] Step4 AAD", step_4_add, aad_len);
    print_hex("[DEBUG] Step4 Plaintext", step_4_input, sizeof(step_4_input));

    psa_status_t status = crypto_aes_gcm_encrypt(ctx->hichain_context.sessionKey, sizeof(ctx->hichain_context.sessionKey),
                                        nonce, sizeof(nonce),
                                        step_4_add, aad_len,
                                        step_4_input, sizeof(step_4_input),
                                        encResult, encResultBufLen, &encResultOutLen);

    if (status != SUCCESS) {
        printf("[0128] Step4 Encrypt failed: %d\n", status);
        free(encResult);
        return -1;
    }

    // [DEBUG] 打印 Step 4 加密结果
    print_hex("[DEBUG] Step4 Encrypted Output (Cipher + Tag)", encResult, encResultOutLen);
    printf("--- [0128] Step 4 Encryption End ---\n\n");

    size_t tlv_len = 0;
    uint8_t* tlv = create_step_four(&config, 4, nonce, GCM_NONCE_LEN, encResult, encResultOutLen, &tlv_len);

    if (tlv) {
        // [DEBUG] 打印最终发送的 TLV
        print_hex("[DEBUG] Step4 TLV to send", tlv, tlv_len);
        send_tlv_and_backup(ctx, tlv, tlv_len);
        free(tlv);
    }

    free(encResult);
    return 0;
}

int Handle0128_3(AuthContext_t* ctx, uint8_t* data, int len) {
    int ret = -1;

    // [DEBUG] 打印接收到的原始 payload
    printf("\n--- [0128] Handle0128_3 Start ---\n");
    print_hex("[DEBUG] Received Payload", data, len);

    ResponseData* resp = parse_response(data, len);

    if (resp == NULL) {
        printf("[0128] Error: resp is null\n");
        return -1;
    }

    uint8_t* nonce = resp->data.step3.nonce;
    size_t nonce_len = resp->data.step3.nonce_len;
    uint8_t* encAuthToken = resp->data.step3.enc_auth_token;
    size_t encAuthTokenLen = resp->data.step3.enc_auth_token_len;

    // [DEBUG] 打印解析后的关键密文部分
    print_hex("[DEBUG] Step3 Parsed Nonce", nonce, nonce_len);
    print_hex("[DEBUG] Step3 Parsed EncAuthToken", encAuthToken, encAuthTokenLen);

    // 分配解密缓冲区
    size_t authTokenBufLen = encAuthTokenLen;
    uint8_t* authToken = (uint8_t*)malloc(authTokenBufLen);

    if (authToken == NULL) {
        printf("[0128] Error: OOM\n");
        free_response_data(resp);
        return -1;
    }

    size_t authTokenOutLen = authTokenBufLen;

    // [DEBUG] 打印 Step 3 解密用的所有参数 (Tag Mismatch 此时最容易发生)
    printf("\n--- [0128] Step 3 Decryption Parameters ---\n");
    print_hex("[DEBUG] Step3 SessionKey", ctx->hichain_context.sessionKey, sizeof(ctx->hichain_context.sessionKey));
    print_hex("[DEBUG] Step3 IV/Nonce", nonce, nonce_len);
    // 这里的 Challenge 充当 AAD
    print_hex("[DEBUG] Step3 AAD (Challenge)", ctx->hichain_context.challenge, sizeof(ctx->hichain_context.challenge));
    print_hex("[DEBUG] Step3 Ciphertext Input", encAuthToken, encAuthTokenLen);

    psa_status_t status = crypto_aes_gcm_decrypt(ctx->hichain_context.sessionKey, sizeof(ctx->hichain_context.sessionKey),
                                    nonce, nonce_len,
                                    ctx->hichain_context.challenge, sizeof(ctx->hichain_context.challenge),
                                    encAuthToken, encAuthTokenLen,
                                    authToken, authTokenBufLen, &authTokenOutLen);

    if (status != SUCCESS) {
        printf("[0128] Decrypt AuthToken Failed (Tag Mismatch?): %d\n", status);
        printf("[DEBUG] Verify: Check if SessionKey matches what peer used.\n");
        printf("[DEBUG] Verify: Check if AAD (Challenge) matches exactly.\n");
        goto cleanup;
    }

    printf("[0128] Decryption Success!\n");

    if (authTokenOutLen > sizeof(ctx->secretKey)) {
        printf("[0128] Error: AuthToken too long: %zu > %zu\n", authTokenOutLen, sizeof(ctx->secretKey));
        goto cleanup;
    }

    print_hex("[0128] Decrypted Token (New SecretKey)", authToken, authTokenOutLen);

    // 安全拷贝
    memset(ctx->secretKey, 0, sizeof(ctx->secretKey));
    memcpy(ctx->secretKey, authToken, authTokenOutLen);

    // 更新状态
    ctx->hichain_context.step = 4;
    ctx->current_retry_times = ctx->retry_times;
    ctx->state = AUTH_STATE_WAIT_0128;
    ctx->hichain_context.operationCode = 1;

    // 发送 Step 4
    if (send_hichain_04(ctx) == 0) {
        ret = 0; // 成功
    }

cleanup:
    free(authToken);
    free_response_data(resp);
    return ret;
}