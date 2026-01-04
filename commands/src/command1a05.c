#include "commands/inc/command1a05.h"

#include <stdio.h>
#include <string.h>

#include "hichain_utils.h"
#include "huawei_tlv.h"

extern void send_tlv_and_backup(AuthContext_t* ctx, const uint8_t* data, uint16_t len);

int Handle1a05(AuthContext_t* ctx, uint8_t* data, int len) {
    ctx->current_retry_times = ctx->retry_times;
    ctx->state = AUTH_STATE_WAIT_0131;
    size_t data_len;
    uint8_t* data_0131 = hex_string_to_bytes("010002000300040005000600", &data_len);
    encrypt(0x01, 0x31, &data_0131, &data_len);
    send_tlv_and_backup(ctx, data_0131, data_len);
    free(data_0131);
    return 0;
}
