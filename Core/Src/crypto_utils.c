//
// Created by 19571 on 2025/12/24.
//

#include "crypto_utils.h"

#include "crypto_utils.h"
#include <string.h>
#include <stdlib.h>

/* * 辅助宏：检查 PSA 状态，如果失败则清除密钥并返回
 */
#define CHECK_STATUS(status) \
    if ((status) != PSA_SUCCESS) { \
        goto exit; \
    }

psa_status_t crypto_init(void)
{
    return psa_crypto_init();
}

/* 内部辅助函数：导入 AES 密钥 */
static psa_status_t import_aes_key(const uint8_t *key, size_t key_len, psa_key_id_t *key_id, psa_key_usage_t usage)
{
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_usage_flags(&attributes, usage);
    psa_set_key_algorithm(&attributes, PSA_ALG_ECB_NO_PADDING); // Default alg, will be overridden by usage context usually but type matters
    psa_set_key_type(&attributes, PSA_KEY_TYPE_AES);
    psa_set_key_bits(&attributes, key_len * 8);

    return psa_import_key(&attributes, key, key_len, key_id);
}

/* 内部辅助函数：导入 HMAC 密钥 */
static psa_status_t import_hmac_key(const uint8_t *key, size_t key_len, psa_key_id_t *key_id, psa_algorithm_t alg)
{
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_SIGN_MESSAGE | PSA_KEY_USAGE_VERIFY_MESSAGE);
    psa_set_key_algorithm(&attributes, alg);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_HMAC);

    return psa_import_key(&attributes, key, key_len, key_id);
}

/* 内部辅助函数：导入 Derivation 密钥 (HKDF 等) */
static psa_status_t import_derivation_key(const uint8_t *key, size_t key_len, psa_key_id_t *key_id, psa_algorithm_t alg)
{
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_usage_flags(&attributes, PSA_KEY_USAGE_DERIVE);
    psa_set_key_algorithm(&attributes, alg);
    psa_set_key_type(&attributes, PSA_KEY_TYPE_DERIVE);

    return psa_import_key(&attributes, key, key_len, key_id);
}

/* ================== AES ECB NoPadding ================== */

psa_status_t crypto_aes_ecb_encrypt_nopad(const uint8_t *key, size_t key_len,
                                          const uint8_t *input, size_t input_len,
                                          uint8_t *output, size_t output_size, size_t *output_len)
{
    psa_status_t status;
    psa_key_id_t key_id = 0;
    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;

    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_ENCRYPT);
    psa_set_key_algorithm(&attr, PSA_ALG_ECB_NO_PADDING);
    psa_set_key_type(&attr, PSA_KEY_TYPE_AES);

    status = psa_import_key(&attr, key, key_len, &key_id);
    CHECK_STATUS(status);

    status = psa_cipher_encrypt(key_id, PSA_ALG_ECB_NO_PADDING,
                                input, input_len,
                                output, output_size, output_len);

exit:
    if (key_id != 0) psa_destroy_key(key_id);
    return status;
}

psa_status_t crypto_aes_ecb_decrypt_nopad(const uint8_t *key, size_t key_len,
                                          const uint8_t *input, size_t input_len,
                                          uint8_t *output, size_t output_size, size_t *output_len)
{
    psa_status_t status;
    psa_key_id_t key_id = 0;
    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;

    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_DECRYPT);
    psa_set_key_algorithm(&attr, PSA_ALG_ECB_NO_PADDING);
    psa_set_key_type(&attr, PSA_KEY_TYPE_AES);

    status = psa_import_key(&attr, key, key_len, &key_id);
    CHECK_STATUS(status);

    status = psa_cipher_decrypt(key_id, PSA_ALG_ECB_NO_PADDING,
                                input, input_len,
                                output, output_size, output_len);

exit:
    if (key_id != 0) psa_destroy_key(key_id);
    return status;
}

/* ================== AES CBC PKCS5Padding ================== */

psa_status_t crypto_aes_cbc_encrypt_pad(const uint8_t *key, size_t key_len,
                                        const uint8_t *iv, size_t iv_len,
                                        const uint8_t *input, size_t input_len,
                                        uint8_t *output, size_t output_size, size_t *output_len)
{
    psa_status_t status;
    psa_key_id_t key_id = 0;
    psa_cipher_operation_t operation = PSA_CIPHER_OPERATION_INIT;
    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;

    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_ENCRYPT);
    psa_set_key_algorithm(&attr, PSA_ALG_CBC_PKCS7);
    psa_set_key_type(&attr, PSA_KEY_TYPE_AES);

    status = psa_import_key(&attr, key, key_len, &key_id);
    CHECK_STATUS(status);

    status = psa_cipher_encrypt_setup(&operation, key_id, PSA_ALG_CBC_PKCS7);
    CHECK_STATUS(status);

    status = psa_cipher_set_iv(&operation, iv, iv_len);
    CHECK_STATUS(status);

    status = psa_cipher_update(&operation, input, input_len, output, output_size, output_len);
    CHECK_STATUS(status);

    size_t finish_len = 0;
    status = psa_cipher_finish(&operation, output + *output_len, output_size - *output_len, &finish_len);
    CHECK_STATUS(status);

    *output_len += finish_len;

exit:
    psa_cipher_abort(&operation);
    if (key_id != 0) psa_destroy_key(key_id);
    return status;
}

psa_status_t crypto_aes_cbc_decrypt_pad(const uint8_t *key, size_t key_len,
                                        const uint8_t *iv, size_t iv_len,
                                        const uint8_t *input, size_t input_len,
                                        uint8_t *output, size_t output_size, size_t *output_len)
{
    psa_status_t status;
    psa_key_id_t key_id = 0;
    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;

    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_DECRYPT);
    psa_set_key_algorithm(&attr, PSA_ALG_CBC_PKCS7);
    psa_set_key_type(&attr, PSA_KEY_TYPE_AES);

    status = psa_import_key(&attr, key, key_len, &key_id);
    CHECK_STATUS(status);

    /* 直接使用单步调用，IV 必须包含在 input 的前缀中吗？
       不，psa_cipher_decrypt 文档说：如果算法需要 IV，它通常作为 input 的一部分。
       但是 PSA 的单步接口对于 CBC 模式，通常期望 IV 在密文前面。
       Java 的 `decryptAES_CBC_Pad` 是单独传入 IV 的。

       因此，我们必须使用多步操作 (setup -> set_iv -> update -> finish)。
    */
    psa_cipher_operation_t operation = PSA_CIPHER_OPERATION_INIT;

    status = psa_cipher_decrypt_setup(&operation, key_id, PSA_ALG_CBC_PKCS7);
    CHECK_STATUS(status);

    status = psa_cipher_set_iv(&operation, iv, iv_len);
    CHECK_STATUS(status);

    status = psa_cipher_update(&operation, input, input_len, output, output_size, output_len);
    CHECK_STATUS(status);

    size_t finish_len = 0;
    status = psa_cipher_finish(&operation, output + *output_len, output_size - *output_len, &finish_len);
    CHECK_STATUS(status);

    *output_len += finish_len;

    psa_cipher_abort(&operation);

exit:
    if (key_id != 0) psa_destroy_key(key_id);
    return status;
}

/* ================== AES ECB PKCS5Padding (Manual) ================== */

psa_status_t crypto_aes_ecb_encrypt_pad(const uint8_t *key, size_t key_len,
                                        const uint8_t *input, size_t input_len,
                                        uint8_t *output, size_t output_size, size_t *output_len)
{
    /* PSA 不支持 ECB 的自动 Padding，需要手动实现 PKCS7 */
    psa_status_t status;
    psa_key_id_t key_id = 0;
    uint8_t *padded_input = NULL;

    /* 1. 计算 Padding */
    size_t pad_len = 16 - (input_len % 16);
    size_t total_len = input_len + pad_len;

    if (output_size < total_len) return PSA_ERROR_BUFFER_TOO_SMALL;

    /* 为了简单起见，我们假设 output 缓冲区够大，直接在 output 上构造 padded 数据？
       不行，因为 input 和 output 可能是同一个 buffer (in-place)，或者重叠。
       安全起见，申请临时 buffer。嵌入式下可以使用栈 buffer 如果数据小，
       这里使用 malloc 确保通用性，或者要求调用者保证 output 够大并先拷贝。

       优化：直接拷贝到 output，然后填充 output。前提是 caller 提供了 output buffer。
    */

    /* 拷贝原数据到输出 */
    memcpy(output, input, input_len);

    /* 填充 PKCS7 */
    for (size_t i = 0; i < pad_len; i++) {
        output[input_len + i] = (uint8_t)pad_len;
    }

    /* 2. 执行 ECB NoPadding 加密 */
    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_ENCRYPT);
    psa_set_key_algorithm(&attr, PSA_ALG_ECB_NO_PADDING);
    psa_set_key_type(&attr, PSA_KEY_TYPE_AES);

    status = psa_import_key(&attr, key, key_len, &key_id);
    if (status != PSA_SUCCESS) return status; // 早期返回不需要 goto，因为还没有分配资源

    /* 加密可以原地进行 (in-place) */
    size_t enc_len = 0;
    status = psa_cipher_encrypt(key_id, PSA_ALG_ECB_NO_PADDING,
                                output, total_len, // 输入是刚刚填充好的 output
                                output, output_size, // 输出覆盖回去
                                &enc_len);

    *output_len = enc_len;

    if (key_id != 0) psa_destroy_key(key_id);
    return status;
}

psa_status_t crypto_aes_ecb_decrypt_pad(const uint8_t *key, size_t key_len,
                                        const uint8_t *input, size_t input_len,
                                        uint8_t *output, size_t output_size, size_t *output_len)
{
    psa_status_t status;
    psa_key_id_t key_id = 0;

    /* 1. 执行 ECB NoPadding 解密 */
    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_DECRYPT);
    psa_set_key_algorithm(&attr, PSA_ALG_ECB_NO_PADDING);
    psa_set_key_type(&attr, PSA_KEY_TYPE_AES);

    status = psa_import_key(&attr, key, key_len, &key_id);
    CHECK_STATUS(status);

    size_t dec_len = 0;
    status = psa_cipher_decrypt(key_id, PSA_ALG_ECB_NO_PADDING,
                                input, input_len,
                                output, output_size,
                                &dec_len);
    CHECK_STATUS(status);

    /* 2. 移除 PKCS7 Padding */
    if (dec_len == 0 || dec_len % 16 != 0) {
        status = PSA_ERROR_INVALID_PADDING;
        goto exit;
    }

    uint8_t pad_val = output[dec_len - 1];

    /* 校验 Padding 合法性 */
    if (pad_val == 0 || pad_val > 16 || pad_val > dec_len) {
        status = PSA_ERROR_INVALID_PADDING;
        goto exit;
    }

    for (size_t i = 0; i < pad_val; i++) {
        if (output[dec_len - 1 - i] != pad_val) {
            status = PSA_ERROR_INVALID_PADDING;
            goto exit;
        }
    }

    *output_len = dec_len - pad_val;

exit:
    if (key_id != 0) psa_destroy_key(key_id);
    return status;
}

/* ================== AES GCM NoPadding ================== */

psa_status_t crypto_aes_gcm_encrypt(const uint8_t *key, size_t key_len,
                                    const uint8_t *iv, size_t iv_len,
                                    const uint8_t *aad, size_t aad_len,
                                    const uint8_t *input, size_t input_len,
                                    uint8_t *output, size_t output_size, size_t *output_len)
{
    psa_status_t status;
    psa_key_id_t key_id = 0;
    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;

    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_ENCRYPT);
    psa_set_key_algorithm(&attr, PSA_ALG_GCM);
    psa_set_key_type(&attr, PSA_KEY_TYPE_AES);

    status = psa_import_key(&attr, key, key_len, &key_id);
    CHECK_STATUS(status);

    /* PSA AEAD 接口会自动处理 GCM Tag，将其附加在密文末尾 */
    status = psa_aead_encrypt(key_id, PSA_ALG_GCM,
                              iv, iv_len,
                              aad, aad_len,
                              input, input_len,
                              output, output_size,
                              output_len);

exit:
    if (key_id != 0) psa_destroy_key(key_id);
    return status;
}

psa_status_t crypto_aes_gcm_decrypt(const uint8_t *key, size_t key_len,
                                    const uint8_t *iv, size_t iv_len,
                                    const uint8_t *aad, size_t aad_len,
                                    const uint8_t *input, size_t input_len,
                                    uint8_t *output, size_t output_size, size_t *output_len)
{
    psa_status_t status;
    psa_key_id_t key_id = 0;
    psa_key_attributes_t attr = PSA_KEY_ATTRIBUTES_INIT;

    psa_set_key_usage_flags(&attr, PSA_KEY_USAGE_DECRYPT);
    psa_set_key_algorithm(&attr, PSA_ALG_GCM);
    psa_set_key_type(&attr, PSA_KEY_TYPE_AES);

    status = psa_import_key(&attr, key, key_len, &key_id);
    CHECK_STATUS(status);

    status = psa_aead_decrypt(key_id, PSA_ALG_GCM,
                              iv, iv_len,
                              aad, aad_len,
                              input, input_len,
                              output, output_size,
                              output_len);

exit:
    if (key_id != 0) psa_destroy_key(key_id);
    return status;
}

/* ================== HMAC SHA256 ================== */

psa_status_t crypto_hmac_sha256(const uint8_t *key, size_t key_len,
                                const uint8_t *input, size_t input_len,
                                uint8_t *output, size_t output_size, size_t *output_len)
{
    psa_status_t status;
    psa_key_id_t key_id = 0;

    status = import_hmac_key(key, key_len, &key_id, PSA_ALG_HMAC(PSA_ALG_SHA_256));
    CHECK_STATUS(status);

    status = psa_mac_compute(key_id, PSA_ALG_HMAC(PSA_ALG_SHA_256),
                             input, input_len,
                             output, output_size,
                             output_len);

exit:
    if (key_id != 0) psa_destroy_key(key_id);
    return status;
}

/* ================== SHA256 Digest ================== */

psa_status_t crypto_sha256(const uint8_t *input, size_t input_len,
                           uint8_t *output, size_t output_size, size_t *output_len)
{
    return psa_hash_compute(PSA_ALG_SHA_256,
                            input, input_len,
                            output, output_size,
                            output_len);
}

/* ================== HKDF SHA256 ================== */

psa_status_t crypto_hkdf_sha256(const uint8_t *secret, size_t secret_len,
                                const uint8_t *salt, size_t salt_len,
                                const uint8_t *info, size_t info_len,
                                uint8_t *output, size_t output_len)
{
    psa_status_t status;
    psa_key_id_t key_id = 0;
    psa_key_derivation_operation_t operation = PSA_KEY_DERIVATION_OPERATION_INIT;

    /* 导入 Secret Key (IKM) */
    status = import_derivation_key(secret, secret_len, &key_id, PSA_ALG_HKDF(PSA_ALG_SHA_256));
    CHECK_STATUS(status);

    /* 设置 HKDF 操作 */
    status = psa_key_derivation_setup(&operation, PSA_ALG_HKDF(PSA_ALG_SHA_256));
    CHECK_STATUS(status);

    /* 提供 Salt */
    if (salt != NULL && salt_len > 0) {
        status = psa_key_derivation_input_bytes(&operation, PSA_KEY_DERIVATION_INPUT_SALT, salt, salt_len);
        CHECK_STATUS(status);
    }

    /* 提供 Secret (IKM) */
    status = psa_key_derivation_input_key(&operation, PSA_KEY_DERIVATION_INPUT_SECRET, key_id);
    CHECK_STATUS(status);

    /* 提供 Info */
    if (info != NULL && info_len > 0) {
        status = psa_key_derivation_input_bytes(&operation, PSA_KEY_DERIVATION_INPUT_INFO, info, info_len);
        CHECK_STATUS(status);
    }

    /* 生成输出 */
    status = psa_key_derivation_output_bytes(&operation, output, output_len);
    CHECK_STATUS(status);

exit:
    psa_key_derivation_abort(&operation);
    if (key_id != 0) psa_destroy_key(key_id);
    return status;
}

/* ================== PBKDF2 HMAC SHA256 ================== */

psa_status_t crypto_pbkdf2_sha256(const char *password, size_t password_len,
                                  const uint8_t *salt, size_t salt_len,
                                  uint32_t iterations,
                                  uint8_t *output, size_t output_len)
{
    psa_status_t status;
    psa_key_id_t key_id = 0;
    psa_key_derivation_operation_t operation = PSA_KEY_DERIVATION_OPERATION_INIT;

    /* 导入 Password 作为密钥 */
    /* 注意：PSA PBKDF2 的输入通常作为 PSA_KEY_DERIVATION_INPUT_PASSWORD 或 INPUT_SECRET */
    status = import_derivation_key((const uint8_t*)password, password_len, &key_id, PSA_ALG_PBKDF2_HMAC(PSA_ALG_SHA_256));
    CHECK_STATUS(status);

    status = psa_key_derivation_setup(&operation, PSA_ALG_PBKDF2_HMAC(PSA_ALG_SHA_256));
    CHECK_STATUS(status);

    /* 设置 Cost (Iterations) */
    status = psa_key_derivation_input_integer(&operation, PSA_KEY_DERIVATION_INPUT_COST, iterations);
    CHECK_STATUS(status);

    /* 提供 Salt */
    status = psa_key_derivation_input_bytes(&operation, PSA_KEY_DERIVATION_INPUT_SALT, salt, salt_len);
    CHECK_STATUS(status);

    /* 提供 Password */
    status = psa_key_derivation_input_key(&operation, PSA_KEY_DERIVATION_INPUT_PASSWORD, key_id);
    CHECK_STATUS(status);

    /* 生成输出 */
    status = psa_key_derivation_output_bytes(&operation, output, output_len);
    CHECK_STATUS(status);

exit:
    psa_key_derivation_abort(&operation);
    if (key_id != 0) psa_destroy_key(key_id);
    return status;
}