#include "commands/inc/command0130.h"

#include <stdio.h>
#include <string.h>

#include "hichain_utils.h"
#include "huawei_tlv.h"

extern void send_tlv_and_backup(AuthContext_t* ctx, const uint8_t* data, uint16_t len);

int Handle0130(AuthContext_t* ctx, uint8_t* data, int len) {
    ctx->current_retry_times = ctx->retry_times;
    ctx->state = AUTH_STATE_WAIT_013F;
    size_t data_len;
    uint8_t* data_013f = hex_string_to_bytes("0102fd17", &data_len);
    encrypt(0x01, 0x3F, &data_013f, &data_len);
    send_tlv_and_backup(ctx, data_013f, data_len);
    free(data_013f);
    return 0;
}