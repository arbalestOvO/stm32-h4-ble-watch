//
// Created by 19571 on 2025/12/26.
//

#include "commands/inc/command0101.h"

#include <stdio.h>
#include <string.h>

#include "huawei_tlv.h"

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
    return 0;
}
