#include "commands/inc/command012c.h"

#include <stdio.h>
#include <string.h>

#include "app_log.h"
#include "crypto_utils.h"
#include "huawei_tlv.h"

extern void send_tlv_and_backup(AuthContext_t* ctx, const uint8_t* data, uint16_t len);

// 2025-12-30 00:41:53.512   855-871   LZX                     nod...n.freeyourgadget.gadgetbridge  I  LZX message 5dc594a09d61a306fda849b88e075d8b679fc73cfca74c44409a7438cd3052ef31f5d60ec197f03d6e09c9cbda254683ae905b15da2944f461e3bbb08e955b88987f62613b517a3cecb11e8dc5de1183
// 2025-12-30 00:41:53.512   855-871   LZX                     nod...n.freeyourgadget.gadgetbridge  I  LZX iv 5b3f88263bc25a8ba0813a3711238730
// 2025-12-30 00:41:53.513   855-871   LZX                     nod...n.freeyourgadget.gadgetbridge  I  LZX pinCode 13524034589334060034385235943736869396922334533706040305257543237526471231264243029125066405346231014793858801099713935323152171


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
    crypto_aes_cbc_decrypt_pad(pin_key, sizeof(pin_key), view_0x02.value, view_0x02.length, view_0x01.value, view_0x01.length, ctx->pinCode, 64, &o_len);
    print_hex("pinCode", ctx->pinCode, o_len);
    ctx->current_retry_times = ctx->retry_times;
    ctx->state = AUTH_STATE_WAIT_0128;
    ctx->hichainStep = 1;
    return 0;
}