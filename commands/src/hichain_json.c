#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "hichain_json.h"
#include "hichain_utils.h"  // 引入工具函数头文件
#include "huawei_tlv.h"

// --- 内部函数：构建基础 JSON 结构 (模拟 BaseStep + createJson) ---
// 返回 root 对象，并通过 payload_out 指针返回 payload 对象的引用以便后续添加字段
static cJSON* create_base_json(RequestConfig *config, int message_id, cJSON **payload_out, cJSON **value_out) {
    cJSON *root = cJSON_CreateObject(); // This is 'value' in Java context (root of structure)
    cJSON *json_payload = cJSON_CreateObject();
    cJSON *version = cJSON_CreateObject();

    // 对应 Java: messageId |= 0x10;
    if (config->operation_code == OPERATION_CODE_02) {
        message_id |= 0x10;
    }

    // 对应 Java: version.put...
    cJSON_AddStringToObject(version, "minVersion", "1.0.0");
    cJSON_AddStringToObject(version, "currentVersion", "2.0.16");

    // 对应 Java: jsonPayload.put("version", version)
    cJSON_AddItemToObject(json_payload, "version", version);

    // 对应 Java: value.put...
    cJSON_AddNumberToObject(root, "authForm", 0x00);
    cJSON_AddItemToObject(root, "payload", json_payload); // Attach payload to root
    cJSON_AddStringToObject(root, "groupAndModuleVersion", "2.0.1");
    cJSON_AddNumberToObject(root, "message", message_id);

    // 对应 Java: if (operationCode == 0x01) { ... }
    if (config->operation_code == OPERATION_CODE_01) {
        char req_id_str[32];
        sprintf(req_id_str, "%lld", (long long)config->request_id);
        
        cJSON_AddStringToObject(root, "requestId", req_id_str);
        cJSON_AddStringToObject(root, "groupId", config->group_id);
        cJSON_AddStringToObject(root, "groupName", "health_group_name");
        cJSON_AddNumberToObject(root, "groupOp", 2);
        cJSON_AddNumberToObject(root, "groupType", 256);

        // Java: new String(selfAuthId, StandardCharsets.UTF_8)
        // 假设 self_auth_id 是合法的 C 字符串或字节流
        char *device_id_str = (char*)malloc(config->self_auth_id_len + 1);
        if (device_id_str) {
            memcpy(device_id_str, config->self_auth_id, config->self_auth_id_len);
            device_id_str[config->self_auth_id_len] = '\0';
            cJSON_AddStringToObject(root, "peerDeviceId", device_id_str);
            cJSON_AddStringToObject(root, "connDeviceId", device_id_str);
            free(device_id_str);
        }

        cJSON_AddStringToObject(root, "appId", "com.huawei.health");
        cJSON_AddStringToObject(root, "ownerName", "");
    }

    *payload_out = json_payload;
    if (value_out) *value_out = root;
    return root;
}

// --- 内部函数：序列化并清理 ---
static uint8_t* finalize_json_and_process_tlv(cJSON *root, RequestConfig *config, cJSON *value_obj) {
    char *json_str = cJSON_PrintUnformatted(root);
    size_t tlv_sz = 3 + strlen(json_str) + 3 + 15;
    uint8_t *tlv = (uint8_t*)malloc(tlv_sz);
    htlv_writer_t writer;
    htlv_writer_init(&writer, tlv, tlv_sz);
    htlv_write_string(&writer, 0x01, json_str);
    htlv_write_bool(&writer, 0x02, true);
    htlv_write_long(&writer, 0x03, -500);
    // --- TODO: TLV 实现区域 ---
    // 对应 Java: this.tlv = new HuaweiTLV().put(0x01, value.toString())...
    // 此处你需要使用 json_str, config->operation_code, config->request_id 来构建 TLV buffer
    // 例如:
    // ByteBuffer tlv = tlv_create();
    // tlv_put_string(tlv, 0x01, json_str);
    // tlv_put_byte(tlv, 0x02, config->operation_code);
    // ...
    // -------------------------

    cJSON_Delete(root); // 释放整个 JSON 树
    return NULL;
}

// --- Step 1 实现 ---
uint8_t* create_step_one(RequestConfig *config, int message_id, uint8_t *iso_salt, size_t salt_len, uint8_t *seed, size_t seed_len) {
    cJSON *payload = NULL;
    cJSON *value = NULL;
    cJSON *root = create_base_json(config, message_id, &payload, &value);

    char *salt_hex = bytes_to_hex(iso_salt, salt_len);
    char *auth_hex = bytes_to_hex(config->self_auth_id, config->self_auth_id_len);
    char *seed_hex = bytes_to_hex(seed, seed_len);

    if (salt_hex) cJSON_AddStringToObject(payload, "isoSalt", salt_hex);
    if (auth_hex) cJSON_AddStringToObject(payload, "peerAuthId", auth_hex);
    cJSON_AddNumberToObject(payload, "operationCode", config->operation_code);
    if (seed_hex) cJSON_AddStringToObject(payload, "seed", seed_hex);
    cJSON_AddNumberToObject(payload, "peerUserType", 0x00);

    if (config->operation_code == OPERATION_CODE_02) {
        cJSON_AddStringToObject(payload, "pkgName", "com.huawei.devicegroupmanage");
        cJSON_AddStringToObject(payload, "serviceType", config->group_id);
        cJSON_AddNumberToObject(payload, "keyLength", 0x20);
        cJSON_AddBoolToObject(value, "isDeviceLevel", cJSON_False);
    }

    free(salt_hex);
    free(auth_hex);
    free(seed_hex);

    return finalize_json_and_process_tlv(root, config, value);
}

// --- Step 2 实现 ---
uint8_t* create_step_two(RequestConfig *config, int message_id, uint8_t *token, size_t token_len) {
    cJSON *payload = NULL;
    cJSON *value = NULL;
    cJSON *root = create_base_json(config, message_id, &payload, &value);

    char *auth_hex = bytes_to_hex(config->self_auth_id, config->self_auth_id_len);
    char *token_hex = bytes_to_hex(token, token_len);

    if (auth_hex) cJSON_AddStringToObject(payload, "peerAuthId", auth_hex);
    if (token_hex) cJSON_AddStringToObject(payload, "token", token_hex);

    if (config->operation_code == OPERATION_CODE_02) {
        cJSON_AddBoolToObject(value, "isDeviceLevel", cJSON_False);
    }

    free(auth_hex);
    free(token_hex);

    return finalize_json_and_process_tlv(root, config, value);
}

// --- Step 3 实现 ---
uint8_t* create_step_three(RequestConfig *config, int message_id, uint8_t *nonce, size_t nonce_len, uint8_t *enc_data, size_t enc_len) {
    cJSON *payload = NULL;
    cJSON *value = NULL;
    cJSON *root = create_base_json(config, message_id, &payload, &value);

    char *nonce_hex = bytes_to_hex(nonce, nonce_len);
    char *enc_hex = bytes_to_hex(enc_data, enc_len);

    if (nonce_hex) cJSON_AddStringToObject(payload, "nonce", nonce_hex);
    if (enc_hex) cJSON_AddStringToObject(payload, "encData", enc_hex);

    free(nonce_hex);
    free(enc_hex);

    return finalize_json_and_process_tlv(root, config, value);
}

// --- Step 4 实现 ---
uint8_t* create_step_four(RequestConfig *config, int message_id, uint8_t *nonce, size_t nonce_len, uint8_t *enc_result, size_t result_len) {
    cJSON *payload = NULL;
    cJSON *value = NULL;
    // Java注释: if (opCode == 0x01) createJson(4) else createJson(3). 
    // 这里我们传入 message_id，由调用者决定传 3 还是 4.
    cJSON *root = create_base_json(config, message_id, &payload, &value);

    char *nonce_hex = bytes_to_hex(nonce, nonce_len);
    char *result_hex = bytes_to_hex(enc_result, result_len);

    if (nonce_hex) cJSON_AddStringToObject(payload, "nonce", nonce_hex);
    if (result_hex) cJSON_AddStringToObject(payload, "encResult", result_hex);
    cJSON_AddNumberToObject(payload, "operationCode", config->operation_code);

    free(nonce_hex);
    free(result_hex);

    return finalize_json_and_process_tlv(root, config, value);
}

// --- Response Parsing 实现 ---

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

ResponseData* parse_response_json(const char *json_str) {
    // TODO: Java 中有 parseTlv 逻辑来判断 type 是 0x00 (JSON) 还是其他 (Step 4 Raw String)。
    // 这里假设传入的是已经从 TLV 中提取出的 JSON 字符串部分（Type 0x00）
    
    cJSON *root = cJSON_Parse(json_str);
    if (!root) return NULL;

    ResponseData *resp = (ResponseData*)calloc(1, sizeof(ResponseData));
    
    // Java: jsonPayload = value.getJSONObject("payload");
    cJSON *payload = cJSON_GetObjectItemCaseSensitive(root, "payload");
    if (!payload) {
        cJSON_Delete(root);
        free(resp);
        return NULL; // Invalid format
    }

    cJSON *error_code = cJSON_GetObjectItemCaseSensitive(payload, "errorCode");
    if (error_code) {
        resp->error_code = error_code->valueint;
    }

    // Determine Step based on fields
    if (cJSON_HasObjectItem(payload, "isoSalt")) {
        resp->step = 1;
        cJSON *item;
        
        item = cJSON_GetObjectItem(payload, "isoSalt");
        if (item) resp->data.step1.iso_salt = hex_string_to_bytes(item->valuestring, &resp->data.step1.iso_salt_len);
        
        item = cJSON_GetObjectItem(payload, "peerAuthId");
        if (item) resp->data.step1.peer_auth_id = hex_string_to_bytes(item->valuestring, &resp->data.step1.peer_auth_id_len);
        
        item = cJSON_GetObjectItem(payload, "peerUserType");
        if (item) resp->data.step1.peer_user_type = item->valueint;
        
        item = cJSON_GetObjectItem(payload, "token");
        if (item) resp->data.step1.token = hex_string_to_bytes(item->valuestring, &resp->data.step1.token_len);

    } else if (cJSON_HasObjectItem(payload, "returnCodeMac")) {
        resp->step = 2;
        cJSON *item = cJSON_GetObjectItem(payload, "returnCodeMac");
        if (item) resp->data.step2.return_code_mac = hex_string_to_bytes(item->valuestring, &resp->data.step2.return_code_mac_len);

    } else if (cJSON_HasObjectItem(payload, "encAuthToken")) {
        resp->step = 3;
        cJSON *item;
        
        item = cJSON_GetObjectItem(payload, "nonce");
        if (item) resp->data.step3.nonce = hex_string_to_bytes(item->valuestring, &resp->data.step3.nonce_len);

        item = cJSON_GetObjectItem(payload, "encAuthToken");
        if (item) resp->data.step3.enc_auth_token = hex_string_to_bytes(item->valuestring, &resp->data.step3.enc_auth_token_len);
    } 
    
    cJSON_Delete(root);
    return resp;
}