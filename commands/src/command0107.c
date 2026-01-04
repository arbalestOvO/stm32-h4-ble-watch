#include "commands/inc/command0107.h"

#include <stdio.h>
#include <string.h>

#include "huawei_tlv.h"
#include "TimeUtils.h"

#define TIME_LEN 10

extern void send_tlv_and_backup(AuthContext_t* ctx, const uint8_t* data, uint16_t len);

int Handle0107(AuthContext_t* ctx, uint8_t* data, int len) {
    ctx->current_retry_times = ctx->retry_times;
    ctx->state = AUTH_STATE_WAIT_0105;
    int time = Time_GetUnixTimestamp() / 1000;
    uint8_t* data_0105 = (uint8_t*)malloc(TIME_LEN);
    htlv_writer_t writer;
    htlv_writer_init(&writer, data_0105, TIME_LEN);
    htlv_write_int(&writer, 0x01, time);
    htlv_write_short(&writer, 0x02, 0x0800);
    size_t data_len = writer.offset;
    encrypt(0x01, 0x05, &data_0105, &data_len);
    send_tlv_and_backup(ctx, data_0105, data_len);
    free(data_0105);
    return 0;
}
