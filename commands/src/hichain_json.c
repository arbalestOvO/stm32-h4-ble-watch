#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <inttypes.h>

/* 引入你的头文件 */
#include "hichain_type.h"
#include "hichain_utils.h" /* 假设 hex_string_to_bytes 和 bytes_to_hex 在这里 */
#include "huawei_tlv.h"    /* 假设 htlv_ 相关函数在这里 */

/* 引入 JSMN (如果你是单文件编译，需要定义 JSMN_STATIC 或者在某处实现它) */
/* 建议在项目中某一个 .c 文件中定义 #define JSMN_PARENT_LINKS (可选) 和 #include "jsmn.h" */
#include "jsmn.h"

#define MAX_JSON_BUFFER 2048
#define MAX_JSMN_TOKENS 128

// --- 辅助：将 Hex 写入 buffer ---
static int append_hex(char *buf, size_t buf_maxlen, const uint8_t *data, size_t len) {
    if (!data || len == 0) return 0;
    size_t written = 0;
    for (size_t i = 0; i < len; i++) {
        if (written + 2 >= buf_maxlen) break; // 防止溢出
        written += snprintf(buf + written, buf_maxlen - written, "%02X", data[i]);
    }
    return (int)written;
}

// --- 辅助：封装 TLV ---
static uint8_t* package_tlv(const char *json_str, RequestConfig *config, size_t *out_len) {
    size_t json_len = strlen(json_str);
    // Head(2) + String(1+len) + Op(1+1+1) + ReqId(1+1+8) = 15 + len
    size_t tlv_sz = 3 + json_len + 3 + 15;

    uint8_t *tlv = (uint8_t*)malloc(tlv_sz);
    if (!tlv) { if(out_len)*out_len=0; return NULL; }

    htlv_writer_t writer;
    htlv_writer_init(&writer, tlv, tlv_sz);

    // Header
    writer.buffer[0] = 0x01; writer.buffer[1] = 0x28; writer.offset += 2;

    // Tag 0x01: JSON
    htlv_write_string(&writer, 0x01, json_str);

    // Tag 0x02: Op Code
    uint8_t op = (uint8_t)config->operation_code;
    htlv_write_tag(&writer, 0x02, &op, 1);

    // Tag 0x03: Request ID
    htlv_write_long(&writer, 0x03, config->request_id);

    if (out_len) *out_len = writer.offset;
    return writer.buffer;
}

// --- 核心：JSON 构建器 ---
// 定义回调函数类型，用于填充 payload 内部差异化数据
typedef void (*PayloadAppender)(char *buf, size_t max, size_t *offset, void *ctx);

void uint64_to_str(uint64_t value, char *buffer) {
    char temp[21];
    char *p = temp;
    if (value == 0) {
        *buffer++ = '0';
        *buffer = 0;
        return;
    }
    while (value > 0) {
        *p++ = (value % 10) + '0';
        value /= 10;
    }
    while (p > temp) {
        *buffer++ = *--p;
    }
    *buffer = 0;
}


static uint8_t* build_full_json(RequestConfig *config, int message_id, PayloadAppender appender, void *ctx, size_t *out_len) {
    char *buf = (char*)malloc(MAX_JSON_BUFFER);
    if (!buf) return NULL;

    size_t offset = 0;
    size_t max = MAX_JSON_BUFFER;

    // 1. 头部公共信息
    offset += snprintf(buf + offset, max - offset,
        "{\"authForm\":0,\"payload\":{\"version\":{\"minVersion\":\"1.0.0\",\"currentVersion\":\"2.0.16\"}");

    // 2. 插入 Payload 具体内容
    if (appender) {
        offset += snprintf(buf + offset, max - offset, ","); // 添加分隔符
        appender(buf, max, &offset, ctx);
    }

    // 3. Payload 结束 & 外层公共字段
    offset += snprintf(buf + offset, max - offset, "},\"groupAndModuleVersion\":\"2.0.1\"");

    // Message ID 处理
    int final_msg_id = message_id;
    if (config->operation_code == OPERATION_CODE_02) {
        final_msg_id |= 0x10;
    }
    offset += snprintf(buf + offset, max - offset, ",\"message\":%d", final_msg_id);

    // 4. Operation Code 01 特有字段
    if (config->operation_code == OPERATION_CODE_01) {
        char num_buf[24];
        uint64_to_str(config->request_id, num_buf);
        offset += snprintf(buf + offset, max - offset, ",\"requestId\":\"%s\"", num_buf);
        // 注意：RequestConfig 中 group_id 是 const char*
        offset += snprintf(buf + offset, max - offset, ",\"groupId\":\"%s\"", config->group_id ? config->group_id : "");
        offset += snprintf(buf + offset, max - offset, ",\"groupName\":\"health_group_name\",\"groupOp\":2,\"groupType\":256");

        // PeerDeviceId (Hex) - 源自 self_auth_id

        // offset += snprintf(buf + offset, max - offset, ",\"peerDeviceId\":\"");
        // offset += append_hex(buf + offset, max - offset, config->self_auth_id, config->self_auth_id_len);

        offset += snprintf(buf + offset, max - offset, ",\"peerDeviceId\":\"%s\"", config->self_auth_id ? (char*)config->self_auth_id : "");
        // ConnDeviceId (同上)

        // offset += snprintf(buf + offset, max - offset, "\",\"connDeviceId\":\"");
        // offset += append_hex(buf + offset, max - offset, config->self_auth_id, config->self_auth_id_len);

        offset += snprintf(buf + offset, max - offset, ",\"connDeviceId\":\"%s\"", config->self_auth_id ? (char*)config->self_auth_id : "");

        offset += snprintf(buf + offset, max - offset, ",\"appId\":\"com.huawei.health\",\"ownerName\":\"\"");
    }
    if (config->operation_code == OPERATION_CODE_02 && (message_id == 1 || message_id == 2)) {
        offset += snprintf(buf + offset, max - offset, ",\"isDeviceLevel\":false");
    }
    // 5. 结束
    offset += snprintf(buf + offset, max - offset, "}");

    // 6. 打包 TLV
    uint8_t *ret = package_tlv(buf, config, out_len);
    free(buf);
    return ret;
}

// --- Step 1 实现 ---
typedef struct {
    uint8_t *salt; size_t salt_len;
    uint8_t *seed; size_t seed_len;
    RequestConfig *cfg;
} S1Ctx;

static void s1_appender(char *buf, size_t max, size_t *offset, void *ctx) {
    S1Ctx *d = (S1Ctx*)ctx;
    // isoSalt
    *offset += snprintf(buf + *offset, max - *offset, "\"isoSalt\":\"");
    *offset += append_hex(buf + *offset, max - *offset, d->salt, d->salt_len);

    // peerAuthId (这里根据原逻辑，填的是自己的 authId)
    *offset += snprintf(buf + *offset, max - *offset, "\",\"peerAuthId\":\"");
    *offset += append_hex(buf + *offset, max - *offset, d->cfg->self_auth_id, d->cfg->self_auth_id_len);

    // operationCode
    *offset += snprintf(buf + *offset, max - *offset, "\",\"operationCode\":%d", d->cfg->operation_code);

    // seed
    if (d->seed) {
        *offset += snprintf(buf + *offset, max - *offset, ",\"seed\":\"");
        *offset += append_hex(buf + *offset, max - *offset, d->seed, d->seed_len);
        *offset += snprintf(buf + *offset, max - *offset, "\"");
    }
    *offset += snprintf(buf + *offset, max - *offset, ",\"peerUserType\":0");

    if (d->cfg->operation_code == OPERATION_CODE_02) {
        *offset += snprintf(buf + *offset, max - *offset,
            ",\"pkgName\":\"com.huawei.devicegroupmanage\",\"serviceType\":\"%s\",\"keyLength\":32",
            d->cfg->group_id ? d->cfg->group_id : "");
    }
}

uint8_t* create_step_one(RequestConfig *config, int message_id, uint8_t *iso_salt, size_t salt_len, uint8_t *seed, size_t seed_len, size_t *out_len) {
    S1Ctx ctx = { iso_salt, salt_len, seed, seed_len, config };
    return build_full_json(config, message_id, s1_appender, &ctx, out_len);
}

// --- Step 2 实现 ---
typedef struct {
    uint8_t *token; size_t token_len;
    RequestConfig *cfg;
} S2Ctx;

static void s2_appender(char *buf, size_t max, size_t *offset, void *ctx) {
    S2Ctx *d = (S2Ctx*)ctx;
    *offset += snprintf(buf + *offset, max - *offset, "\"peerAuthId\":\"");
    *offset += append_hex(buf + *offset, max - *offset, d->cfg->self_auth_id, d->cfg->self_auth_id_len);

    *offset += snprintf(buf + *offset, max - *offset, "\",\"token\":\"");
    *offset += append_hex(buf + *offset, max - *offset, d->token, d->token_len);
    *offset += snprintf(buf + *offset, max - *offset, "\"");
}

uint8_t* create_step_two(RequestConfig *config, int message_id, uint8_t *token, size_t token_len, size_t *out_len) {
    S2Ctx ctx = { token, token_len, config };
    return build_full_json(config, message_id, s2_appender, &ctx, out_len);
}

// --- Step 3 实现 ---
typedef struct {
    uint8_t *nonce; size_t nonce_len;
    uint8_t *enc; size_t enc_len;
} S3Ctx;

static void s3_appender(char *buf, size_t max, size_t *offset, void *ctx) {
    S3Ctx *d = (S3Ctx*)ctx;
    *offset += snprintf(buf + *offset, max - *offset, "\"nonce\":\"");
    *offset += append_hex(buf + *offset, max - *offset, d->nonce, d->nonce_len);

    *offset += snprintf(buf + *offset, max - *offset, "\",\"encData\":\"");
    *offset += append_hex(buf + *offset, max - *offset, d->enc, d->enc_len);
    *offset += snprintf(buf + *offset, max - *offset, "\"");
}

uint8_t* create_step_three(RequestConfig *config, int message_id, uint8_t *nonce, size_t nonce_len, uint8_t *enc_data, size_t enc_len, size_t *out_len) {
    S3Ctx ctx = { nonce, nonce_len, enc_data, enc_len };
    return build_full_json(config, message_id, s3_appender, &ctx, out_len);
}

// --- Step 4 实现 ---
typedef struct {
    uint8_t *nonce; size_t nonce_len;
    uint8_t *res; size_t res_len;
    RequestConfig *cfg;
} S4Ctx;

static void s4_appender(char *buf, size_t max, size_t *offset, void *ctx) {
    S4Ctx *d = (S4Ctx*)ctx;
    *offset += snprintf(buf + *offset, max - *offset, "\"nonce\":\"");
    *offset += append_hex(buf + *offset, max - *offset, d->nonce, d->nonce_len);

    *offset += snprintf(buf + *offset, max - *offset, "\",\"encResult\":\"");
    *offset += append_hex(buf + *offset, max - *offset, d->res, d->res_len);

    *offset += snprintf(buf + *offset, max - *offset, "\",\"operationCode\":%d", d->cfg->operation_code);
}

uint8_t* create_step_four(RequestConfig *config, int message_id, uint8_t *nonce, size_t nonce_len, uint8_t *enc_result, size_t result_len, size_t *out_len) {
    S4Ctx ctx = { nonce, nonce_len, enc_result, result_len, config };
    return build_full_json(config, message_id, s4_appender, &ctx, out_len);
}


// ================== 解析部分 (JSMN) ==================

// 辅助：判断 token key 是否等于字符串 s
static int json_eq(const char *json, jsmntok_t *tok, const char *s) {
    if (tok->type == JSMN_STRING && (int)strlen(s) == tok->end - tok->start &&
        strncmp(json + tok->start, s, tok->end - tok->start) == 0) {
        return 0;
    }
    return -1;
}

// 辅助：提取 token 内容为 Hex Bytes
static uint8_t* extract_hex(const char *json, jsmntok_t *t, size_t *out_len) {
    if (!t || t->type != JSMN_STRING) return NULL;
    int len = t->end - t->start;

    // 需要拷贝出来加 \0 才能给 hex_string_to_bytes 用
    char *tmp = (char*)malloc(len + 1);
    if (!tmp) return NULL;
    memcpy(tmp, json + t->start, len);
    tmp[len] = '\0';

    uint8_t *ret = hex_string_to_bytes(tmp, out_len);
    free(tmp);
    return ret;
}

void free_response_data(ResponseData *resp) {
    if (!resp) return;
    if (resp->step == 1) {
        free(resp->data.step1.iso_salt);
        free(resp->data.step1.peer_auth_id);
        free(resp->data.step1.token);
    } else if (resp->step == 2) {
        free(resp->data.step2.return_code_mac);
    } else if (resp->step == 3) {
        free(resp->data.step3.nonce);
        free(resp->data.step3.enc_auth_token);
    } else if (resp->step == 4) {
        free(resp->data.step4.data);
    }
    free(resp);
}

ResponseData* parse_response(const uint8_t *data, size_t len) {
    ResponseData *resp = (ResponseData*)calloc(1, sizeof(ResponseData));
    if (!resp) return NULL;

    htlv_view_t view;
    // 检查是否有 Type (Tag 0x04)
    uint8_t type = 0x00;
    if (htlv_find(data, len, 0x04, &view) == HTLV_OK && view.length > 0) {
        type = view.value[0];
    }

    // Tag 0x01: Data
    if (htlv_find(data, len, 0x01, &view) != HTLV_OK) {
        free(resp); return NULL;
    }

    // 复制出字符串
    char *json_str = (char*)malloc(view.length + 1);
    if (!json_str) { free(resp); return NULL; }
    htlv_read_string(&view, json_str, view.length + 1);

    // 如果 type != 0，直接作为 Raw String (Step 4) 处理
    if (type != 0x00) {
        resp->step = 4;
        resp->data.step4.data = json_str; // 转移所有权，不需要 strdup，后面 free_response 释放
        return resp;
    }

    // 解析 JSON
    jsmn_parser parser;
    jsmntok_t t[MAX_JSMN_TOKENS];
    jsmn_init(&parser);
    int r = jsmn_parse(&parser, json_str, strlen(json_str), t, MAX_JSMN_TOKENS);

    if (r < 1 || t[0].type != JSMN_OBJECT) {
        // 解析失败
        free(json_str); free(resp); return NULL;
    }

    // 寻找 "payload" 对象
    int payload_idx = -1;
    for (int i = 1; i < r; i++) {
        if (json_eq(json_str, &t[i], "payload") == 0) {
            payload_idx = i + 1; // payload 的 value
            break;
        }
        // 简单跳过：如果当前是 Key，下一个是 Value。但 Value 可能是 Object
        // 这里的线性扫描假设 JSON 扁平，或者我们只匹配 Key。
        // JSMN 的结果是线性的，包含所有子 Token。
        // 为了准确，必须检查 parent。
        // 由于是简单 JSON，直接查找 "payload" 只要不是嵌套很深一般没问题。
    }

    if (payload_idx != -1 && payload_idx < r && t[payload_idx].type == JSMN_OBJECT) {
        // 在 payload 对象范围内查找 Key
        int end = t[payload_idx].end;

        // 查找 ErrorCode
        for (int i = payload_idx + 1; i < r; i++) {
            if (t[i].start >= end) break; // 超出 payload 范围

            if (json_eq(json_str, &t[i], "errorCode") == 0) {
                // 提取 int
                char num_buf[16] = {0};
                int nlen = t[i+1].end - t[i+1].start;
                if (nlen < 15) {
                    memcpy(num_buf, json_str + t[i+1].start, nlen);
                    resp->error_code = atoi(num_buf);
                }
            }

            // 判断 Step (通过特有字段)
            if (json_eq(json_str, &t[i], "isoSalt") == 0) {
                resp->step = 1;
                resp->data.step1.iso_salt = extract_hex(json_str, &t[i+1], &resp->data.step1.iso_salt_len);
            }
            else if (json_eq(json_str, &t[i], "peerAuthId") == 0 && resp->step == 1) {
                resp->data.step1.peer_auth_id = extract_hex(json_str, &t[i+1], &resp->data.step1.peer_auth_id_len);
            }
            else if (json_eq(json_str, &t[i], "token") == 0 && resp->step == 1) {
                resp->data.step1.token = extract_hex(json_str, &t[i+1], &resp->data.step1.token_len);
            }
            else if (json_eq(json_str, &t[i], "peerUserType") == 0 && resp->step == 1) {
                 char num_buf[16] = {0};
                 int nlen = t[i+1].end - t[i+1].start;
                 if (nlen < 15) {
                     memcpy(num_buf, json_str + t[i+1].start, nlen);
                     resp->data.step1.peer_user_type = atoi(num_buf);
                 }
            }
            else if (json_eq(json_str, &t[i], "returnCodeMac") == 0) {
                resp->step = 2;
                resp->data.step2.return_code_mac = extract_hex(json_str, &t[i+1], &resp->data.step2.return_code_mac_len);
            }
            else if (json_eq(json_str, &t[i], "encAuthToken") == 0) {
                resp->step = 3;
                resp->data.step3.enc_auth_token = extract_hex(json_str, &t[i+1], &resp->data.step3.enc_auth_token_len);
            }
            else if (json_eq(json_str, &t[i], "nonce") == 0 && resp->step == 3) {
                resp->data.step3.nonce = extract_hex(json_str, &t[i+1], &resp->data.step3.nonce_len);
            }
        }
    } else {
        // 如果没有 payload 字段，视为 Raw String Step 4
        resp->step = 4;
        resp->data.step4.data = strdup(json_str); // 需要 strdup 因为下面要 free json_str
    }

    free(json_str);
    return resp;
}