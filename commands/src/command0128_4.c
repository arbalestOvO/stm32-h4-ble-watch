#include "commands/inc/command0128_4.h"

#include <stdio.h>
#include <string.h>

#include "hichain_json.h"
#include "hichain_utils.h"
#include "huawei_tlv.h"
#include "random_utils.h"

extern void send_tlv_and_backup(AuthContext_t* ctx, const uint8_t* data, uint16_t len);

int Handle0128_4(AuthContext_t* ctx, uint8_t* data, int len) {
    RequestConfig config = {0};
    ctx->hichain_context.step = 5;
    ctx->current_retry_times = ctx->retry_times;
    ctx->state = AUTH_STATE_WAIT_0128;
    ctx->hichain_context.operationCode = 2;
    hichain_RequestConfig_Init(&config, ctx->hichain_context.requestId, ctx->hichain_context.operationCode);
    size_t tlv_len;
    Random_GetByteArray(ctx->hichain_context.seed, 0x20);
    Random_GetByteArray(ctx->hichain_context.randSelf, 0x10);
    uint8_t* tlv_data = create_step_one(&config, 0x01, ctx->hichain_context.randSelf, 0x10, ctx->hichain_context.seed, 0x20, &tlv_len);
    send_tlv_and_backup(ctx, tlv_data, tlv_len);
    free(tlv_data);
    return 0;
}
