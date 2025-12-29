#ifndef HICHAIN_JSON_H
#define HICHAIN_JSON_H

#include "cJSON.h"
#include <stdint.h>
#include <stdbool.h>

// 模拟 Java 中的常量
#define OPERATION_CODE_01 0x01
#define OPERATION_CODE_02 0x02

// 模拟 Request 类的基础字段
typedef struct {
    int operation_code;
    int64_t request_id;
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

// --- API 声明 ---

// 请求构建函数 (返回 tlv，调用者需要 free)
uint8_t* create_step_one(RequestConfig *config, int message_id, uint8_t *iso_salt, size_t salt_len, uint8_t *seed, size_t seed_len);
uint8_t* create_step_two(RequestConfig *config, int message_id, uint8_t *token, size_t token_len);
uint8_t* create_step_three(RequestConfig *config, int message_id, uint8_t *nonce, size_t nonce_len, uint8_t *enc_data, size_t enc_len);
uint8_t* create_step_four(RequestConfig *config, int message_id, uint8_t *nonce, size_t nonce_len, uint8_t *enc_result, size_t result_len);

// 响应解析函数
ResponseData* parse_response_json(const char *json_str);
void free_response_data(ResponseData *resp);

#endif // HICHAIN_JSON_H