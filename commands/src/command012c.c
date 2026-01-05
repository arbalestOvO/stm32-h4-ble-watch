#include "commands/inc/command012c.h"

#include <stdio.h>
#include <string.h>

#include "app_log.h"
#include "crypto_utils.h"
#include "hichain_json.h"
#include "hichain_utils.h"
#include "huawei_tlv.h"
#include "random_utils.h"
#include "ui_interface.h"

extern void send_tlv_and_backup(AuthContext_t* ctx, const uint8_t* data, uint16_t len);

// 2025-12-30 00:41:53.512   855-871   LZX                     nod...n.freeyourgadget.gadgetbridge  I  LZX message 5dc594a09d61a306fda849b88e075d8b679fc73cfca74c44409a7438cd3052ef31f5d60ec197f03d6e09c9cbda254683ae905b15da2944f461e3bbb08e955b88987f62613b517a3cecb11e8dc5de1183
// 2025-12-30 00:41:53.512   855-871   LZX                     nod...n.freeyourgadget.gadgetbridge  I  LZX iv 5b3f88263bc25a8ba0813a3711238730
// 2025-12-30 00:41:53.513   855-871   LZX                     nod...n.freeyourgadget.gadgetbridge  I  LZX pinCode 13524034589334060034385235943736869396922334533706040305257543237526471231264243029125066405346231014793858801099713935323152171

void send_hichain_start(AuthContext_t* ctx) {
    ui_show_notify_safe(true, false, "鉴权中...");
    printf("begin hichain req: %llu\n", ctx->hichain_context.requestId);
    printf("Request operationCode: %d - step: %d\n", ctx->hichain_context.operationCode, ctx->hichain_context.step);
    Random_GetByteArray(ctx->hichain_context.seed, 0x20);
    Random_GetByteArray(ctx->hichain_context.randSelf, 0x10);
    RequestConfig config;
    hichain_RequestConfig_Init(&config, ctx->hichain_context.requestId, ctx->hichain_context.operationCode);
    memcpy(ctx->hichain_context.authIdSelf, config.self_auth_id, LEN_AUTH_ID_SELF);
    size_t tlv_len;
    uint8_t* tlv_data = create_step_one(&config, 0x01, ctx->hichain_context.randSelf, 0x10, ctx->hichain_context.seed, 0x20, &tlv_len);
    send_tlv_and_backup(ctx, tlv_data, tlv_len);
    free(tlv_data);
}


int Handle012c(AuthContext_t* ctx, uint8_t* data, int len) {
    static uint8_t pin_key[] = {0x70,0xfb,0x6c,0x24,0x03,0x5f,0xdb,0x55,0x2f,0x38,0x89,0x8a,0xee,0xde,0x3f,0x69};
    htlv_view_t view_0x01;
    if (htlv_find(data, len, 0x01, &view_0x01) != HTLV_OK) {
        printf("012C NOT FOUND 0x01\n");
        return -1;
    }
    htlv_view_t view_0x02;
    if (htlv_find(data, len, 0x02,  &view_0x02) != HTLV_OK) {
        printf("012C NOT FOUND 0x02\n");
        return -1;
    }
    size_t o_len;
    print_hex("iv", view_0x02.value, view_0x02.length);
    print_hex("message", view_0x01.value, view_0x01.length);
    uint8_t output[96];
    size_t output_size = sizeof(output);
    crypto_aes_cbc_decrypt_pad(pin_key, sizeof(pin_key), view_0x02.value, view_0x02.length, view_0x01.value, view_0x01.length, output, output_size, &o_len);
    memcpy(ctx->pinCode, output, o_len);
    print_hex("pinCode", ctx->pinCode, o_len);
    ctx->current_retry_times = ctx->retry_times;
    ctx->state = AUTH_STATE_WAIT_0128;
    ctx->hichain_context.step = 1;
    ctx->hichain_context.operationCode = 1;
    send_hichain_start(ctx);
    return 0;
}