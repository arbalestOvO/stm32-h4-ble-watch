#include "commands/inc/command0105.h"

#include <stdio.h>
#include <string.h>

#include "hichain_utils.h"
#include "huawei_tlv.h"

extern void send_tlv_and_backup(AuthContext_t* ctx, const uint8_t* data, uint16_t len);

int Handle0105(AuthContext_t* ctx, uint8_t* data, int len) {
    ctx->current_retry_times = ctx->retry_times;
    ctx->state = AUTH_STATE_WAIT_0102;
    size_t data_len;
    uint8_t* data_0102 = hex_string_to_bytes("012b02030405060708090a0b0c0d0e0f101112131415161718191a1b1d202223242526272a2b2d2e3032333435", &data_len);
    encrypt(0x01, 0x02, &data_0102, &data_len);
    send_tlv_and_backup(ctx, data_0102, data_len);
    free(data_0102);
    return 0;
}
