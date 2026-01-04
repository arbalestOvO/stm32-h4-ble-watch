//
// Created by 19571 on 2025/12/26.
//

#include "commands/inc/command0101.h"

#include <stdio.h>
#include <string.h>

#include "huawei_tlv.h"

extern void send_tlv_and_backup(AuthContext_t* ctx, const uint8_t* data, uint16_t len);

static uint8_t HICHAIN_FLAG = 0x04;
static uint8_t HICHAIN_FLAG_01 = 0x01;
static uint8_t HICHAIN_FLAG_00 = 0x00;
void send_0133_cmd(AuthContext_t* ctx) {
    uint8_t buffer[64] = {0x01, 0x33};
    htlv_writer_t writer;
    htlv_writer_init(&writer, buffer, sizeof(buffer));
    writer.offset += 2;
    htlv_write_tag(&writer, 0x01, &HICHAIN_FLAG, 1);
    htlv_write_tag(&writer, 0x02, &HICHAIN_FLAG_01, 1);
    htlv_write_tag(&writer, 0x03, &HICHAIN_FLAG_01, 1);
    htlv_write_tag(&writer, 0x04, &HICHAIN_FLAG_00, 1);
    htlv_write_string(&writer, 0x05, ctx->uuid);
    htlv_write_tag(&writer, 0x06, NULL, 0);
    htlv_write_string(&writer, 0x07, "STM32");
    ctx->current_retry_times = ctx->retry_times;
    ctx->state = AUTH_STATE_WAIT_0133;
    send_tlv_and_backup(ctx, buffer, writer.offset);
}

int Handle0101(AuthContext_t* ctx, uint8_t* data, int len)
{
    htlv_view_t view;
    if (htlv_find(data, len, 0x02, &view) == HTLV_OK) {
        ctx->mfs = htlv_read_short(&view);
    } else {
        printf("[0101 WARN] not found 0x02\n");
    }

    if (htlv_find(data, len, 0x03, &view) == HTLV_OK) {
        ctx->mtu = htlv_read_short(&view);
    } else {
        printf("[0101 WARN] not found 0x03\n");
    }

    if (htlv_find(data, len, 0x05, &view) == HTLV_OK) {
        ctx->auth_version = view.value[1];
        memcpy(ctx->server_nonce, view.value + 2, 16);
    } else {
        printf("[0101 WARN] not found 0x05\n");
    }

    if (htlv_find(data, len, 0x07, &view) == HTLV_OK) {
        ctx->device_support_type = view.value[0];
    } else {
        printf("[0101 WARN] not found 0x07\n");
    }

    if (htlv_find(data, len, 0x08, &view) == HTLV_OK) {
        ctx->auth_algo = view.value[0];
    } else {
        printf("[0101 WARN] not found 0x08\n");
    }

    if (htlv_find(data, len, 0x09, &view) == HTLV_OK) {
        ctx->bond_state = view.value[0];
    } else {
        printf("[0101 WARN] not found 0x09\n");
    }

    if (htlv_find(data, len, 0x0C, &view) == HTLV_OK) {
        ctx->encrypt_method = view.value[0];
    } else {
        printf("[0101 WARN] not found 0x0C\n");
    }
    printf("update 0101 ok\n");
    send_0133_cmd(ctx);
    return 0;
}
