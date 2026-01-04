#include "commands/inc/command013f.h"

#include <stdio.h>
#include <string.h>

#include "hichain_utils.h"
#include "huawei_tlv.h"

extern void send_tlv_and_backup(AuthContext_t* ctx, const uint8_t* data, uint16_t len);

int Handle013f(AuthContext_t* ctx, uint8_t* data, int len) {
    ctx->current_retry_times = ctx->retry_times;
    ctx->state = AUTH_STATE_WAIT_013E;
    size_t data_len;
    uint8_t* data_013E = hex_string_to_bytes("010101021548554157454920574154434820475420342d363130030100", &data_len);
    encrypt(0x01, 0x3E, &data_013E, &data_len);
    send_tlv_and_backup(ctx, data_013E, data_len);
    free(data_013E);
    tx_thread_sleep(30);
    ctx->state = AUTH_STATE_WAIT_013D;
    uint8_t* data_013D = hex_string_to_bytes("0100", &data_len);
    encrypt(0x01, 0x3D, &data_013D, &data_len);
    send_tlv_and_backup(ctx, data_013D, data_len);
    free(data_013D);
    return 0;
}