#include "commands/inc/command0135.h"

#include <stdio.h>
#include <string.h>

#include "huawei_tlv.h"
#include "ui_interface.h"

extern void send_tlv_and_backup(AuthContext_t* ctx, const uint8_t* data, uint16_t len);

int Handle0135(AuthContext_t* ctx, uint8_t* data, int len) {
    ctx->state = AUTH_STATE_AUTHENTICATED;
    ui_show_notify_safe(false, false, "鉴权成功...");
    return 0;
}
