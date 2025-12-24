//
// Created by 19571 on 2025/12/24.
//

#ifndef ABOLUO_EXIT_CRYPTO_UTILS_H
#define ABOLUO_EXIT_CRYPTO_UTILS_H


#include <stddef.h>
#include <stdint.h>
#include "psa/crypto.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief 初始化 Crypto 库 (psa_crypto_init)
 * * @return psa_status_t
 */
psa_status_t crypto_init(void);

/**
 * @brief AES-ECB NoPadding 加密
 * 对应 Java: encryptAES (AES/ECB/NoPadding)
 * * @param key           密钥 (16/24/32 bytes)
 * @param key_len       密钥长度
 * @param input         输入数据 (必须是 16 字节倍数)
 * @param input_len     输入长度
 * @param output        输出缓冲区
 * @param output_size   输出缓冲区最大大小 (建议 input_len)
 * @param output_len    实际输出长度
 * @return psa_status_t
 */
psa_status_t crypto_aes_ecb_encrypt_nopad(const uint8_t *key, size_t key_len,
                                          const uint8_t *input, size_t input_len,
                                          uint8_t *output, size_t output_size, size_t *output_len);

/**
 * @brief AES-ECB NoPadding 解密
 * 对应 Java: decryptAES (AES/ECB/NoPadding)
 */
psa_status_t crypto_aes_ecb_decrypt_nopad(const uint8_t *key, size_t key_len,
                                          const uint8_t *input, size_t input_len,
                                          uint8_t *output, size_t output_size, size_t *output_len);

/**
 * @brief AES-CBC PKCS5Padding (PKCS7) 加密
 * 对应 Java: encryptAES_CBC_Pad (AES/CBC/PKCS5Padding)
 * * @param iv            初始化向量 (16 bytes)
 * @param iv_len        IV 长度
 */
psa_status_t crypto_aes_cbc_encrypt_pad(const uint8_t *key, size_t key_len,
                                        const uint8_t *iv, size_t iv_len,
                                        const uint8_t *input, size_t input_len,
                                        uint8_t *output, size_t output_size, size_t *output_len);

/**
 * @brief AES-CBC PKCS5Padding (PKCS7) 解密
 * 对应 Java: decryptAES_CBC_Pad (AES/CBC/PKCS5Padding)
 */
psa_status_t crypto_aes_cbc_decrypt_pad(const uint8_t *key, size_t key_len,
                                        const uint8_t *iv, size_t iv_len,
                                        const uint8_t *input, size_t input_len,
                                        uint8_t *output, size_t output_size, size_t *output_len);

/**
 * @brief AES-ECB PKCS5Padding 加密 (手动填充实现)
 * 对应 Java: encryptAES_ECB_Pad (AES/ECB/PKCS5Padding)
 */
psa_status_t crypto_aes_ecb_encrypt_pad(const uint8_t *key, size_t key_len,
                                        const uint8_t *input, size_t input_len,
                                        uint8_t *output, size_t output_size, size_t *output_len);

/**
 * @brief AES-ECB PKCS5Padding 解密 (手动去填充实现)
 * 对应 Java: decryptAES_ECB_Pad (AES/ECB/PKCS5Padding)
 */
psa_status_t crypto_aes_ecb_decrypt_pad(const uint8_t *key, size_t key_len,
                                        const uint8_t *input, size_t input_len,
                                        uint8_t *output, size_t output_size, size_t *output_len);

/**
 * @brief AES-GCM NoPadding 加密
 * 对应 Java: encryptAES_GCM_NoPad
 * Tag 会自动附加在密文末尾 (符合 Java doFinal 行为)
 * * @param aad           附加验证数据 (可以为 NULL)
 */
psa_status_t crypto_aes_gcm_encrypt(const uint8_t *key, size_t key_len,
                                    const uint8_t *iv, size_t iv_len,
                                    const uint8_t *aad, size_t aad_len,
                                    const uint8_t *input, size_t input_len,
                                    uint8_t *output, size_t output_size, size_t *output_len);

/**
 * @brief AES-GCM NoPadding 解密
 * 对应 Java: decryptAES_GCM_NoPad
 */
psa_status_t crypto_aes_gcm_decrypt(const uint8_t *key, size_t key_len,
                                    const uint8_t *iv, size_t iv_len,
                                    const uint8_t *aad, size_t aad_len,
                                    const uint8_t *input, size_t input_len,
                                    uint8_t *output, size_t output_size, size_t *output_len);

/**
 * @brief HMAC-SHA256 计算
 * 对应 Java: calcHmacSha256
 */
psa_status_t crypto_hmac_sha256(const uint8_t *key, size_t key_len,
                                const uint8_t *input, size_t input_len,
                                uint8_t *output, size_t output_size, size_t *output_len);

/**
 * @brief SHA-256 摘要
 * 对应 Java: digest
 */
psa_status_t crypto_sha256(const uint8_t *input, size_t input_len,
                           uint8_t *output, size_t output_size, size_t *output_len);

/**
 * @brief HKDF-SHA256 密钥派生
 * 对应 Java: hkdfSha256
 */
psa_status_t crypto_hkdf_sha256(const uint8_t *secret, size_t secret_len,
                                const uint8_t *salt, size_t salt_len,
                                const uint8_t *info, size_t info_len,
                                uint8_t *output, size_t output_len);

/**
 * @brief PBKDF2-HMAC-SHA256 密钥派生
 * 对应 Java: pbkdf2Sha256
 */
psa_status_t crypto_pbkdf2_sha256(const char *password, size_t password_len,
                                  const uint8_t *salt, size_t salt_len,
                                  uint32_t iterations,
                                  uint8_t *output, size_t output_len);

#endif //ABOLUO_EXIT_CRYPTO_UTILS_H