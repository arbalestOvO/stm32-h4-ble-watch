#include "commands/inc/command0128.h"

#include <stdio.h>
#include <string.h>

#include "command0128_1.h"
#include "command0128_2.h"
#include "command0128_3.h"
#include "command0128_4.h"
#include "command0128_5.h"
#include "command0128_6.h"
#include "command0128_7.h"
#include "huawei_tlv.h"

extern void send_tlv_and_backup(AuthContext_t *ctx, const uint8_t *data, uint16_t len);

int Handle0128(AuthContext_t *ctx, uint8_t *data, int len) {
    switch (ctx->hichain_context.step) {
        case 1:
            return Handle0128_1(ctx, data, len);
        case 2:
            return Handle0128_2(ctx, data, len);
        case 3:
            return Handle0128_3(ctx, data, len);
        case 4:
            return Handle0128_4(ctx, data, len);
        case 5:
            return Handle0128_5(ctx, data, len);
        case 6:
            return Handle0128_6(ctx, data, len);
        case 7:
            return Handle0128_7(ctx, data, len);
        default:
            printf("Handle0128: unknown step %d\n", ctx->hichain_context.step);
            return -1;
    }
}
