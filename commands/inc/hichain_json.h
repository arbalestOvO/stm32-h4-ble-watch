#ifndef HICHAIN_JSON_H
#define HICHAIN_JSON_H

#include "cJSON.h"
#include <stdint.h>
#include <stdbool.h>
#include "hichain_type.h"

/**
 * 构建 Step 1 请求数据 (TLV)
 * @param out_len [输出] 返回生成的二进制数据长度
 * @return 新分配的二进制 TLV 数据，需要调用者 free()
 */
uint8_t* create_step_one(RequestConfig *config, int message_id, uint8_t *iso_salt, size_t salt_len, uint8_t *seed, size_t seed_len, size_t *out_len);

/**
 * 构建 Step 2 请求数据 (TLV)
 */
uint8_t* create_step_two(RequestConfig *config, int message_id, uint8_t *token, size_t token_len, size_t *out_len);

/**
 * 构建 Step 3 请求数据 (TLV)
 */
uint8_t* create_step_three(RequestConfig *config, int message_id, uint8_t *nonce, size_t nonce_len, uint8_t *enc_data, size_t enc_len, size_t *out_len);

/**
 * 构建 Step 4 请求数据 (TLV)
 */
uint8_t* create_step_four(RequestConfig *config, int message_id, uint8_t *nonce, size_t nonce_len, uint8_t *enc_result, size_t result_len, size_t *out_len);

ResponseData* parse_response(const uint8_t *data, size_t len);

void free_response_data(ResponseData *resp);

#endif // HICHAIN_JSON_H