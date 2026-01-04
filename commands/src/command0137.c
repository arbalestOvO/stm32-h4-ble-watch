#include "commands/inc/command0137.h"

#include <stdio.h>
#include <string.h>

#include "hichain_utils.h"
#include "huawei_tlv.h"

extern void send_tlv_and_backup(AuthContext_t* ctx, const uint8_t* data, uint16_t len);

int Handle0137(AuthContext_t* ctx, uint8_t* data, int len) {
    ctx->current_retry_times = ctx->retry_times;
    ctx->state = AUTH_STATE_WAIT_1A05;
    size_t data_len;
    uint8_t* data_1a05 = hex_string_to_bytes("010100030101", &data_len);
    encrypt(0x1a, 0x05, &data_1a05, &data_len);
    send_tlv_and_backup(ctx, data_1a05, data_len);
    free(data_1a05);
    return 0;
}
