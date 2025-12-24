//
// Created by 19571 on 2025/12/24.
//

#ifndef ABOLUO_EXIT_TEST_CRYPTO_H
#define ABOLUO_EXIT_TEST_CRYPTO_H

#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include "crypto_utils.h"

static int pass_case = 0;
static int failed_case = 0;
static int all_case = 0;

/* 辅助函数：打印十六进制 */
static void print_hex(const char *label, const uint8_t *data, size_t len) {
    printf("%s (%zu bytes): ", label, len);
    for (size_t i = 0; i < len; i++) {
        printf("%02X", data[i]);
    }
    printf("\n");
}

/* 辅助函数：比较数据 */
static void check_result(const char *test_name, const uint8_t *expected, const uint8_t *actual, size_t len) {
    all_case ++;
    if (memcmp(expected, actual, len) == 0) {
        pass_case ++;
        printf("PASS: %s\n", test_name);
    } else {
        failed_case++;
        printf("FAIL: %s\n", test_name);
        print_hex("Expected", expected, len);
        print_hex("Actual  ", actual, len);
    }
}

static void test_aes_ecb_nopadding(void) {
    printf("\n=== Test AES-ECB NoPadding ===\n");
    uint8_t key[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
    uint8_t input[16] = "0123456789ABCDEF"; // 正好 16 字节
    uint8_t encrypted[32];
    uint8_t decrypted[32];
    size_t enc_len, dec_len;

    psa_status_t status = crypto_aes_ecb_encrypt_nopad(key, sizeof(key), input, sizeof(input), encrypted, sizeof(encrypted), &enc_len);
    if (status != PSA_SUCCESS) { printf("Encrypt Error: %ld\n", status); return; }
    print_hex("Ciphertext", encrypted, enc_len);

    status = crypto_aes_ecb_decrypt_nopad(key, sizeof(key), encrypted, enc_len, decrypted, sizeof(decrypted), &dec_len);
    if (status != PSA_SUCCESS) { printf("Decrypt Error: %ld\n", status); return; }

    check_result("AES-ECB-NoPad Consistency", input, decrypted, sizeof(input));
}

static void test_aes_ecb_pkcs5padding(void) {
    printf("\n=== Test AES-ECB PKCS5Padding (Manual) ===\n");
    uint8_t key[16] = {0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x10};
    uint8_t input[] = "Hello World"; // 11 字节，需要填充 5 字节
    uint8_t encrypted[32];
    uint8_t decrypted[32];
    size_t enc_len, dec_len;

    psa_status_t status = crypto_aes_ecb_encrypt_pad(key, sizeof(key), input, sizeof(input) - 1, encrypted, sizeof(encrypted), &enc_len); // -1 去掉 null 终止符
    if (status != PSA_SUCCESS) { printf("Encrypt Error: %ld\n", status); return; }
    print_hex("Ciphertext (Padded)", encrypted, enc_len);

    status = crypto_aes_ecb_decrypt_pad(key, sizeof(key), encrypted, enc_len, decrypted, sizeof(decrypted), &dec_len);
    if (status != PSA_SUCCESS) { printf("Decrypt Error: %ld\n", status); return; }

    // 打印解密出的字符串（手动添加 NULL 方便 printf）
    decrypted[dec_len] = '\0';
    printf("Decrypted Text: %s\n", decrypted);

    check_result("AES-ECB-PKCS5 Consistency", (uint8_t*)"Hello World", decrypted, 11);
}

static void test_aes_cbc_pkcs5padding(void) {
    printf("\n=== Test AES-CBC PKCS5Padding ===\n");
    uint8_t key[16] = {0x2B, 0x7E, 0x15, 0x16, 0x28, 0xAE, 0xD2, 0xA6, 0xAB, 0xF7, 0x15, 0x88, 0x09, 0xCF, 0x4F, 0x3C};
    uint8_t iv[16]  = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07, 0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F};
    uint8_t input[] = "This is a test message for CBC mode."; // 36 bytes
    uint8_t encrypted[64];
    uint8_t decrypted[64];
    size_t enc_len, dec_len;

    // input size 不包含结尾的 \0
    size_t msg_len = strlen((char*)input);

    psa_status_t status = crypto_aes_cbc_encrypt_pad(key, sizeof(key), iv, sizeof(iv), input, msg_len, encrypted, sizeof(encrypted), &enc_len);
    if (status != PSA_SUCCESS) { printf("Encrypt Error: %ld\n", status); return; }
    print_hex("Ciphertext", encrypted, enc_len);

    status = crypto_aes_cbc_decrypt_pad(key, sizeof(key), iv, sizeof(iv), encrypted, enc_len, decrypted, sizeof(decrypted), &dec_len);
    if (status != PSA_SUCCESS) { printf("Decrypt Error: %ld\n", status); return; }

    decrypted[dec_len] = '\0';
    printf("Decrypted Text: %s\n", decrypted);
    check_result("AES-CBC-PKCS5 Consistency", input, decrypted, msg_len);
}

static void test_aes_gcm(void) {
    printf("\n=== Test AES-GCM NoPadding ===\n");
    uint8_t key[32] = {0xFE, 0xFF, 0xE9, 0x92, 0x86, 0x65, 0x73, 0x1C, 0x6D, 0x6A, 0x8F, 0x94, 0x67, 0x30, 0x83, 0x08,
                       0xFE, 0xFF, 0xE9, 0x92, 0x86, 0x65, 0x73, 0x1C, 0x6D, 0x6A, 0x8F, 0x94, 0x67, 0x30, 0x83, 0x08}; // 256 bit key
    uint8_t iv[12]  = {0xCA, 0xFE, 0xBA, 0xBE, 0xFA, 0xCE, 0xDB, 0xAD, 0xDE, 0xCA, 0xF8, 0x88};
    uint8_t aad[]   = "AAD Data";
    uint8_t input[] = "GCM Secret Data";
    uint8_t encrypted[64];
    uint8_t decrypted[64];
    size_t enc_len, dec_len;
    size_t msg_len = strlen((char*)input);

    psa_status_t status = crypto_aes_gcm_encrypt(key, sizeof(key), iv, sizeof(iv), aad, strlen((char*)aad), input, msg_len, encrypted, sizeof(encrypted), &enc_len);
    if (status != PSA_SUCCESS) { printf("Encrypt Error: %ld\n", status); return; }
    print_hex("Ciphertext (with Tag)", encrypted, enc_len);

    status = crypto_aes_gcm_decrypt(key, sizeof(key), iv, sizeof(iv), aad, strlen((char*)aad), encrypted, enc_len, decrypted, sizeof(decrypted), &dec_len);
    if (status != PSA_SUCCESS) { printf("Decrypt Error: %ld\n", status); return; }

    decrypted[dec_len] = '\0';
    printf("Decrypted Text: %s\n", decrypted);
    check_result("AES-GCM Consistency", input, decrypted, msg_len);
}

static void test_sha256(void) {
    printf("\n=== Test SHA-256 ===\n");
    uint8_t input[] = "abc";
    uint8_t output[32];
    size_t out_len;
    // Expected hash for "abc"
    uint8_t expected[32] = {
        0xBA, 0x78, 0x16, 0xBF, 0x8F, 0x01, 0xCF, 0xEA, 0x41, 0x41, 0x40, 0xDE, 0x5D, 0xAE, 0x22, 0x23,
        0xB0, 0x03, 0x61, 0xA3, 0x96, 0x17, 0x7A, 0x9C, 0xB4, 0x10, 0xFF, 0x61, 0xF2, 0x00, 0x15, 0xAD
    };

    psa_status_t status = crypto_sha256(input, 3, output, sizeof(output), &out_len);
    if (status != PSA_SUCCESS) { printf("SHA256 Error: %ld\n", status); return; }

    check_result("SHA-256 Check", expected, output, 32);
}

static void test_hmac_sha256(void) {
    printf("\n=== Test HMAC-SHA256 ===\n");
    uint8_t key[] = "key";
    uint8_t input[] = "The quick brown fox jumps over the lazy dog";
    uint8_t output[32];
    size_t out_len;

    // HMAC-SHA256("key", "The quick brown fox jumps over the lazy dog")
    uint8_t expected[32] = {
        0xF7, 0xBC, 0x83, 0xF4, 0x30, 0x53, 0x84, 0x24, 0xB1, 0x32, 0x98, 0xE6, 0xAA, 0x6F, 0xB1, 0x43,
        0xEF, 0x4D, 0x59, 0xA1, 0x49, 0x46, 0x17, 0x59, 0x97, 0x47, 0x9D, 0xBC, 0x2D, 0x1A, 0x3C, 0xD8
    };

    psa_status_t status = crypto_hmac_sha256(key, 3, input, strlen((char*)input), output, sizeof(output), &out_len);
    if (status != PSA_SUCCESS) { printf("HMAC Error: %ld\n", status); return; }

    check_result("HMAC-SHA256 Check", expected, output, 32);
}

static void test_hkdf_sha256(void) {
    printf("\n=== Test HKDF-SHA256 ===\n");
    uint8_t ikm[22] = {0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b,0x0b};
    uint8_t salt[13] = {0x00,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x08,0x09,0x0a,0x0b,0x0c};
    uint8_t info[10] = {0xf0,0xf1,0xf2,0xf3,0xf4,0xf5,0xf6,0xf7,0xf8,0xf9};
    uint8_t output[42]; // Request 42 bytes

    // RFC 5869 Test Case 1
    uint8_t expected[42] = {
        0x3c,0xb2,0x5f,0x25,0xfa,0xac,0xd5,0x7a,0x90,0x43,0x4f,0x64,0xd0,0x36,0x2f,0x2a,
        0x2d,0x2d,0x0a,0x90,0xcf,0x1a,0x5a,0x4c,0x5d,0xb0,0x2d,0x56,0xec,0xc4,0xc5,0xbf,
        0x34,0x00,0x72,0x08,0xd5,0xb8,0x87,0x18,0x58,0x65
    };

    psa_status_t status = crypto_hkdf_sha256(ikm, sizeof(ikm), salt, sizeof(salt), info, sizeof(info), output, sizeof(output));
    if (status != PSA_SUCCESS) { printf("HKDF Error: %ld\n", status); return; }

    check_result("HKDF-SHA256 Check", expected, output, sizeof(output));
}

static void test_pbkdf2_sha256(void) {
    printf("\n=== Test PBKDF2-HMAC-SHA256 ===\n");
    // Password: "password"
    // Salt: "salt"
    // Iterations: 1
    // DKLen: 32
    char *password = "password";
    uint8_t salt[] = "salt";
    uint8_t output[32];
    // 0x12,0x0F,0xB6,0xCF,0xFC,0xF8,0xB3,0x2C,0x43,0xE7,0x22,0x52,0x56,0xC4,0xF8,0x37,0xA8,0x65,0x48,0xC9,0x2C,0xCC,0x35,0x48,0x08,0x05,0x98,0x7C,0xB7,0x0B,0xE1,0x7B
    uint8_t expected[32] = {
        0x12,0x0F,0xB6,0xCF,0xFC,0xF8,0xB3,0x2C,0x43,0xE7,0x22,0x52,0x56,0xC4,0xF8,0x37,0xA8,0x65,0x48,0xC9,0x2C,0xCC,0x35,0x48,0x08,0x05,0x98,0x7C,0xB7,0x0B,0xE1,0x7B
    };

    psa_status_t status = crypto_pbkdf2_sha256(password, strlen(password), salt, strlen((char*)salt), 1, output, sizeof(output));
    if (status != PSA_SUCCESS) { printf("PBKDF2 Error: %ld\n", status); return; }

    check_result("PBKDF2 Check (Iter=1)", expected, output, 32);
}

/* 导出函数 */
void test_all(void) {
    printf("\n>>> STARTING CRYPTO TESTS <<<\n");

    // 1. 初始化 PSA Crypto
    psa_status_t status = crypto_init();
    if (status != PSA_SUCCESS) {
        printf("Crypto Init Failed: %ld\n", status);
        return;
    }
    printf("Crypto Init Success.\n");

    // 2. 执行各个测试
    test_aes_ecb_nopadding();
    test_aes_ecb_pkcs5padding();
    test_aes_cbc_pkcs5padding();
    test_aes_gcm();
    test_sha256();
    test_hmac_sha256();
    test_hkdf_sha256();
    test_pbkdf2_sha256();
    printf("ALL CASE %d, pass case %d, failed case %d", all_case, pass_case, failed_case);
    printf("\n>>> ALL TESTS COMPLETED <<<\n");
}

#endif //ABOLUO_EXIT_TEST_CRYPTO_H