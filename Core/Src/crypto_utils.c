//
// Created by 19571 on 2025/12/24.
//

#include "crypto_utils.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h> // 用于 printf

/*
 * 辅助宏：检查 PSA 状态
 * 修改点：将 status 强转为 (int) 并使用 %d 打印，避免 64 位打印问题
 */
#define CHECK_STATUS(status) \
    if ((status) != PSA_SUCCESS) { \
        printf("[Crypto] Error: %s:%d failed. Status: %d\n", __func__, __LINE__, (int)(status)); \
        goto exit; \
    }

psa_status_t crypto_init(void)
{
    psa_status_t status = psa_crypto_init();
    if (status != PSA_SUCCESS) {
        // 修改点：强转为 int
        printf("[Crypto] Init failed. Status: %d\n", (int)status);
    }
    return status;
}

/* 内部辅助函数：导入 AES 密钥 */
static psa_status_t import_aes_key(const uint8_t *key, size_t key_len, psa_key_id_t *key_id, psa_key_usage_t usage)
{
    psa_key_attributes_t attributes = PSA_KEY_ATTRIBUTES_INIT;
    psa_set_key_usage_flags(&attributes, usage);
    psa_set_key_algorithm(&attributes, PSA_ALG_ECB_NO_PADDING); // Default alg
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
    CHECK_STATUS(status);

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
    CHECK_STATUS(status);

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
    psa_status_t status;
    psa_key_id_t key_id = 0;

    /* 1. 计算 Padding */
    size_t pad_len = 16 - (input_len % 16);
    size_t total_len = input_len + pad_len;

    if (output_size < total_len) {
        // 修改点：size_t 强转 unsigned int，使用 %u
        printf("[Crypto] Error: ECB Encrypt buf too small. Need %u, got %u\n",
               (unsigned int)total_len, (unsigned int)output_size);
        return PSA_ERROR_BUFFER_TOO_SMALL;
    }

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
    if (status != PSA_SUCCESS) {
        // 修改点：强转 int
        printf("[Crypto] Error: ECB Encrypt Import Key failed. Status: %d\n", (int)status);
        return status;
    }

    size_t enc_len = 0;
    status = psa_cipher_encrypt(key_id, PSA_ALG_ECB_NO_PADDING,
                                output, total_len,
                                output, output_size,
                                &enc_len);

    if (status != PSA_SUCCESS) {
        // 修改点：强转 int
        printf("[Crypto] Error: ECB Encrypt execution failed. Status: %d\n", (int)status);
    } else {
        *output_len = enc_len;
    }

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
        // 修改点：强转 unsigned int
        printf("[Crypto] Error: Invalid ECB decrypt length: %u\n", (unsigned int)dec_len);
        status = PSA_ERROR_INVALID_PADDING;
        goto exit;
    }

    uint8_t pad_val = output[dec_len - 1];

    /* 校验 Padding 合法性 */
    if (pad_val == 0 || pad_val > 16 || pad_val > dec_len) {
        // 修改点：强转 unsigned int
        printf("[Crypto] Error: Invalid PKCS7 pad value: %d (dec_len: %u)\n",
               (int)pad_val, (unsigned int)dec_len);
        status = PSA_ERROR_INVALID_PADDING;
        goto exit;
    }

    for (size_t i = 0; i < pad_val; i++) {
        if (output[dec_len - 1 - i] != pad_val) {
            // 修改点：强转 unsigned int
            printf("[Crypto] Error: Bad PKCS7 padding byte at index %u\n", (unsigned int)i);
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

    status = psa_aead_encrypt(key_id, PSA_ALG_GCM,
                              iv, iv_len,
                              aad, aad_len,
                              input, input_len,
                              output, output_size,
                              output_len);
    CHECK_STATUS(status);

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
    CHECK_STATUS(status);

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
    CHECK_STATUS(status);

exit:
    if (key_id != 0) psa_destroy_key(key_id);
    return status;
}

/* ================== SHA256 Digest ================== */

psa_status_t crypto_sha256(const uint8_t *input, size_t input_len,
                           uint8_t *output, size_t output_size, size_t *output_len)
{
    psa_status_t status = psa_hash_compute(PSA_ALG_SHA_256,
                                           input, input_len,
                                           output, output_size,
                                           output_len);
    if (status != PSA_SUCCESS) {
        // 修改点：强转 int
        printf("[Crypto] SHA256 compute failed. Status: %d\n", (int)status);
    }
    return status;
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
    status = import_derivation_key((const uint8_t*)password, password_len, &key_id, PSA_ALG_PBKDF2_HMAC(PSA_ALG_SHA_256));
    CHECK_STATUS(status);

    status = psa_key_derivation_setup(&operation, PSA_ALG_PBKDF2_HMAC(PSA_ALG_SHA_256));
    CHECK_STATUS(status);

    /* 设置 Cost (Iterations) */
    // iterations 是 uint32_t，使用 %u 打印是安全的，这里是参数输入，不需要打印
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