#include "commands/inc/command013e.h"

#include <stdio.h>
#include <string.h>

#include "hichain_utils.h"
#include "huawei_tlv.h"

extern void send_tlv_and_backup(AuthContext_t* ctx, const uint8_t* data, uint16_t len);

int Handle013e(AuthContext_t* ctx, uint8_t* data, int len) {
    ctx->current_retry_times = ctx->retry_times;
    ctx->state = AUTH_STATE_WAIT_013D;
    size_t data_len;
    uint8_t* data_013D = hex_string_to_bytes("0100", &data_len);
    encrypt(0x01, 0x3D, &data_013D, &data_len);
    send_tlv_and_backup(ctx, data_013D, data_len);
    free(data_013D);
    return 0;
}
