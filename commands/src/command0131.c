#include "commands/inc/command0131.h"

#include <stdio.h>
#include <string.h>

#include "hichain_utils.h"
#include "huawei_tlv.h"

extern void send_tlv_and_backup(AuthContext_t* ctx, const uint8_t* data, uint16_t len);

int Handle0131(AuthContext_t* ctx, uint8_t* data, int len) {
    ctx->current_retry_times = ctx->retry_times;
    ctx->state = AUTH_STATE_WAIT_0130;
    size_t data_len;
    uint8_t* data_0130 = hex_string_to_bytes("81815b824c0321736f6674776172655f7570646174655f736572766963655f73746174656d656e74040101051532303233303530382d32303233303530382d302d30060d313736363637393437393134348248031d6465766963655f696e666f726d6174696f6e5f6d616e6167656d656e74040101051532303233303530382d32303233303530382d302d30060d3137363636373934373931343482410316757365725f6c6963656e73655f61677265656d656e74040101051532303233303530382d32303233303530382d302d30060d31373636363739343739313434", &data_len);
    encrypt(0x01, 0x30, &data_0130, &data_len);
    send_tlv_and_backup(ctx, data_0130, data_len);
    free(data_0130);
    return 0;
}
