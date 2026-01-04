#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hichain_utils.h"


char* bytes_to_hex(const uint8_t *src, size_t len) {
    if (!src || len == 0) return NULL;
    // 每个字节占2个字符 + 1个结束符
    char *hex = (char*)malloc(len * 2 + 1);
    if (!hex) return NULL;
    
    for (size_t i = 0; i < len; i++) {
        // Java StringUtils 通常输出大写，这里保持一致
        sprintf(hex + (i * 2), "%02X", src[i]); 
    }
    hex[len * 2] = '\0';
    return hex;
}

uint8_t* hex_string_to_bytes(const char *hex, size_t *out_len) {
    if (!hex) return NULL;
    size_t len = strlen(hex);
    
    // 长度必须是偶数
    if (len % 2 != 0) return NULL;
    
    size_t final_len = len / 2;
    uint8_t *bytes = (uint8_t*)malloc(final_len);
    if (!bytes) return NULL;
    
    for (size_t i = 0; i < final_len; i++) {
        unsigned int byte_val;
        // %02x 可以同时匹配大写和小写
        if (sscanf(hex + 2 * i, "%02x", &byte_val) != 1) {
            free(bytes);
            return NULL;
        }
        bytes[i] = (uint8_t)byte_val;
    }
    
    if (out_len) *out_len = final_len;
    return bytes;
}

void hichain_RequestConfig_Init(RequestConfig *config, uint64_t request_id, uint8_t operation_code) {
    static uint8_t fake_auth_id[] = "7410142703F4FDF544C6EE8A7DD3AC29";
    config->request_id = request_id;
    config->group_id = "7B0BC0CBCE474F6C238D9661C63400B797B166EA7849B3A370FC73A9A236E989";
    config->operation_code = operation_code;
    config->self_auth_id = fake_auth_id;
    config->self_auth_id_len = sizeof(fake_auth_id) - 1;
}
