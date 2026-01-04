//
// Created by 19571 on 2025/12/30.
//

#ifndef ABOLUO_EXIT_HICHAIN_TYPE_H
#define ABOLUO_EXIT_HICHAIN_TYPE_H

#define LEN_SESSION_KEY  32
#define LEN_PSK          32
#define LEN_AUTH_ID_SELF 32
#define LEN_AUTH_ID_PEER 64
#define LEN_RAND         16 // randSelf 和 randPeer 长度相同
#define LEN_SEED         32
#define LEN_CHALLENGE    16
#include <stdint.h>
#include <stddef.h>

// 定义结构体
typedef struct {
    uint8_t operationCode;
    uint8_t step;                       // Java: byte -> C: uint8_t (通常用于状态机)

    uint8_t authIdSelf[LEN_AUTH_ID_SELF]; // len 32
    uint8_t authIdPeer[LEN_AUTH_ID_PEER]; // len 64

    uint8_t randSelf[LEN_RAND];         // len 16
    uint8_t randPeer[LEN_RAND];         // len 16

    uint64_t requestId;                 // Java: long (64位) -> C: uint64_t 或 int64_t

    uint8_t sessionKey[LEN_SESSION_KEY];  // len 32

    // Attributes used once
    uint8_t seed[LEN_SEED];             // len 32
    uint8_t challenge[LEN_CHALLENGE];   // len 16
    uint8_t psk[LEN_PSK];               // len 32

} HiChainContext;

// 模拟 Java 中的常量
#define OPERATION_CODE_01 0x01
#define OPERATION_CODE_02 0x02


// 模拟 Request 类的基础字段
typedef struct {
    int operation_code;
    uint64_t request_id;
    uint8_t *self_auth_id;
    size_t self_auth_id_len;
    const char *group_id;
} RequestConfig;

// 响应数据结构体 - Step 1
typedef struct {
    uint8_t *iso_salt;
    size_t iso_salt_len;
    uint8_t *peer_auth_id;
    size_t peer_auth_id_len;
    int peer_user_type;
    uint8_t *token;
    size_t token_len;
} Step1Data;

// 响应数据结构体 - Step 2
typedef struct {
    uint8_t *return_code_mac;
    size_t return_code_mac_len;
} Step2Data;

// 响应数据结构体 - Step 3
typedef struct {
    uint8_t *nonce;
    size_t nonce_len;
    uint8_t *enc_auth_token;
    size_t enc_auth_token_len;
} Step3Data;

// 响应数据结构体 - Step 4 (TLV String)
typedef struct {
    char *data;
} Step4Data;

// 总响应结构体
typedef struct {
    uint8_t step;       // 1, 2, 3, 4
    int error_code;

    // 联合体存储具体步骤数据
    union {
        Step1Data step1;
        Step2Data step2;
        Step3Data step3;
        Step4Data step4;
    } data;
} ResponseData;

#endif //ABOLUO_EXIT_HICHAIN_TYPE_H