//
// Created by 19571 on 2026/1/2.
//

#include "command013d.h"

#include "hichain_utils.h"

extern void send_tlv_and_backup(AuthContext_t* ctx, const uint8_t* data, uint16_t len);

int Handle013d(AuthContext_t* ctx, uint8_t* data, int len) {
    ctx->current_retry_times = ctx->retry_times;
    ctx->state = AUTH_STATE_WAIT_0135;
    size_t data_len;
    uint8_t* data_0135 = hex_string_to_bytes("010101", &data_len);
    encrypt(0x01, 0x35, &data_0135, &data_len);
    send_tlv_and_backup(ctx, data_0135, data_len);
    free(data_0135);
}
