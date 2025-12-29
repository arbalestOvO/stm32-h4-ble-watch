#include "commands/inc/command0133.h"

#include <stdio.h>
#include <string.h>

#include "huawei_tlv.h"

extern void send_tlv_and_backup(AuthContext_t* ctx, const uint8_t* data, uint16_t len);

int Handle0133(AuthContext_t* ctx, uint8_t* data, int len) {
    ctx->current_retry_times = ctx->retry_times;
    ctx->state = AUTH_STATE_WAIT_012C;
    static uint8_t data_0133[4] = {0x01, 0x2C, 0x01, 0x00};
    send_tlv_and_backup(ctx, data_0133, 4);
    return 0;
}
