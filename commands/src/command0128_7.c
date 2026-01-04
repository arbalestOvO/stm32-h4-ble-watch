#include "commands/inc/command0128_7.h"

#include <stdio.h>
#include <string.h>

#include "crypto_utils.h"
#include "hichain_utils.h"
#include "huawei_tlv.h"

extern void send_tlv_and_backup(AuthContext_t* ctx, const uint8_t* data, uint16_t len);

int Handle0128_7(AuthContext_t* ctx, uint8_t* data, int len) {
    if (!ctx) return -1;

    // --- 1. 准备 Salt (按照 Java 逻辑: randSelf + randPeer) ---
    // Java: ByteBuffer.allocate(...).put(randSelf).put(randPeer)
    size_t len_self = sizeof(ctx->hichain_context.randSelf);
    size_t len_peer = sizeof(ctx->hichain_context.randPeer);
    size_t salt_len = len_self + len_peer;

    // 使用栈内存，避免 malloc/free 带来的泄漏风险 (假设 salt 不会超大)
    uint8_t salt[64];
    if (salt_len > sizeof(salt)) {
        return -1; // 缓冲区溢出保护
    }

    // 严格按照 Java 顺序拷贝
    memcpy(salt, ctx->hichain_context.randSelf, len_self);            // 先 put(randSelf)
    memcpy(salt + len_self, ctx->hichain_context.randPeer, len_peer); // 后 put(randPeer)

    // --- 2. HKDF 密钥派生 ---
    // Java: info = "hichain_return_key"
    static const uint8_t return_info[] = "hichain_return_key";
    size_t returnCodeLen = sizeof(return_info) - 1; // 去掉末尾的 \0

    uint8_t key[32];
    size_t keyLen = sizeof(key);

    // Java: CryptoUtils.hkdfSha256(sessionKey, salt, info, 32);
    crypto_hkdf_sha256(
        ctx->hichain_context.sessionKey, sizeof(ctx->hichain_context.sessionKey),
        salt, salt_len,
        return_info, returnCodeLen,
        key, keyLen
    );

    // --- 3. 更新 Context 密钥 ---
    memcpy(ctx->secretKey, key, keyLen);

    memset(key, 0, keyLen);

    ctx->current_retry_times = ctx->retry_times;
    ctx->state = AUTH_STATE_WAIT_0107;

    size_t data_len = 0;
    uint8_t* data_0107 = hex_string_to_bytes(
        "01000200070009000a001100120016001a001d001e001f002000210022002300",
        &data_len
    );

    if (!data_0107) {
        return -2; // 内存分配失败
    }

    int enc_ret = encrypt(0x01, 0x07, &data_0107, &data_len);
    if (enc_ret != 0) {
        free(data_0107);
        return -3; // 加密失败
    }

    send_tlv_and_backup(ctx, data_0107, data_len);
    free(data_0107);
    return 0;
}