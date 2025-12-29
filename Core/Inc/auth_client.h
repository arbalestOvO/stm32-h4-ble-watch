//
// Created by 19571 on 2025/12/26.
//

#ifndef ABOLUO_EXIT_AUTH_CLIENT_H
#define ABOLUO_EXIT_AUTH_CLIENT_H
#include <stdint.h>

#include "tx_api.h"


typedef enum {
    AUTH_STATUS_OK,
    AUTH_STATUS_ERROR,
    AUTH_STATUS_TIMEOUT,
} AuthResult_t;

typedef enum {
    AUTH_STATE_AUTHENTICATED,
    AUTH_STATE_WAIT_0101,
    AUTH_STATE_FAILED
} AuthState_t;

typedef struct {
    uint16_t id;
    uint8_t* payload;
    int      len;
} MsgEvent_t;

typedef struct {
    uint8_t current_retry_times;
    uint8_t retry_times;
    uint16_t mtu;
    uint16_t mfs;
    uint8_t server_nonce[16];
    uint8_t auth_version;
    uint8_t device_support_type;
    uint8_t auth_algo;
    uint8_t bond_state;
    uint8_t encrypt_method;

    int timeout_ms;
    AuthState_t state;

    TX_QUEUE tx_queue;
    void* queue_mem;
    uint8_t* temp_asm_buf;    // 临时拼凑用的 buffer
    uint32_t temp_asm_len;    // 当前已拼凑长度
    uint8_t  next_fsn;        // 下一个期望的序号


    char mac[16];
    uint8_t *last_buf;
    uint32_t last_len;
} AuthContext_t;

typedef int (*CmdHandlerFunc)(AuthContext_t* ctx, uint8_t* data, int len);

typedef struct {
    AuthState_t     required_state;
    uint16_t        cmd_id;
    CmdHandlerFunc  handler;
} ProtocolEntry_t;

AuthContext_t* AuthContext_Create(char* mac, int timeout_ms, int retryTimes);

AuthResult_t auth(AuthContext_t* context);

void AuthContext_Free(AuthContext_t* context);

void on_tlv_received(uint8_t* data, int len);

#endif //ABOLUO_EXIT_AUTH_CLIENT_H