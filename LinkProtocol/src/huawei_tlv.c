//
// Created by 19571 on 2025/12/25.
//

#include "huawei_tlv.h"
#include <string.h>

// --- VarInt Implementation (Private) ---

// Java代码逻辑: Big-Endian 7-bit VarInt
// 高位字节在 buffer 前面，每个字节最高位(0x80)若为1表示后面还有数据
// Java的 putVarIntValue 逻辑是递归或倒序填充的

static int get_varint_size(uint32_t value) {
    int result = 0;
    do {
        result++;
        value >>= 7;
    } while (value != 0);
    return result;
}

static htlv_result_t write_varint(uint8_t* buf, size_t max_len, uint32_t value, size_t* out_len) {
    int size = get_varint_size(value);
    if ((size_t)size > max_len) return HTLV_ERR_BUFFER_TOO_SMALL;

    // Java logic:
    // result[size - 1] = (byte)(value & 0x7F);
    // Loop backwards...

    buf[size - 1] = (uint8_t)(value & 0x7F);
    uint32_t v = value;
    for (int offset = size - 2; offset >= 0; offset--) {
        v >>= 7;
        buf[offset] = (uint8_t)((v & 0x7F) | 0x80);
    }

    if (out_len) *out_len = size;
    return HTLV_OK;
}

static htlv_result_t read_varint(const uint8_t* buf, size_t max_len, uint32_t* out_val, size_t* consumed) {
    uint32_t result = 0;
    size_t offset = 0;

    while (offset < max_len) {
        uint8_t b = buf[offset];
        result += (b & 0x7F);

        offset++;

        if ((b & 0x80) == 0) {
            *out_val = result;
            *consumed = offset;
            return HTLV_OK;
        }

        // Java: result <<= 7
        // 防止溢出检查可以加，但这里对应Java int，暂时假设协议合法
        result <<= 7;
    }

    return HTLV_ERR_MALFORMED;
}

// --- Iterator Implementation ---

void htlv_iter_init(htlv_iter_t* iter, const uint8_t* buffer, size_t size) {
    iter->buffer = buffer;
    iter->size = size;
    iter->offset = 0;
    memset(&iter->current, 0, sizeof(htlv_view_t));
}

bool htlv_iter_next(htlv_iter_t* iter) {
    if (iter->offset >= iter->size) return false;

    // 1. Tag (1 byte)
    iter->current.tag = iter->buffer[iter->offset];
    iter->offset++;

    // Check for trailing padding (Java code: if parsed == length && tag == 0 break)
    if (iter->offset == iter->size && iter->current.tag == 0) {
        return false;
    }

    // 2. Length (VarInt)
    uint32_t val_len = 0;
    size_t varint_bytes = 0;
    htlv_result_t res = read_varint(iter->buffer + iter->offset,
                                    iter->size - iter->offset,
                                    &val_len, &varint_bytes);

    if (res != HTLV_OK) return false; // Malformed
    iter->offset += varint_bytes;

    // 3. Value
    if (iter->offset + val_len > iter->size) return false; // Buffer overflow

    iter->current.length = val_len;
    iter->current.value = iter->buffer + iter->offset; // Zero Copy: Point to existing buffer

    iter->offset += val_len;
    return true;
}

htlv_result_t htlv_find(const uint8_t* buffer, size_t size, uint8_t tag, htlv_view_t* out_view) {
    htlv_iter_t it;
    HTLV_FOREACH(&it, buffer, size) {
        if (it.current.tag == tag) {
            if (out_view) *out_view = it.current;
            return HTLV_OK;
        }
    }
    return HTLV_ERR_NOT_FOUND;
}

// --- Read Helpers (Big Endian except Double) ---

int32_t htlv_read_int(const htlv_view_t* view) {
    if (view->length < 4) return 0; // Or handle error
    const uint8_t* p = view->value;
    return ((int32_t)p[0] << 24) | ((int32_t)p[1] << 16) | ((int32_t)p[2] << 8) | p[3];
}

int16_t htlv_read_short(const htlv_view_t* view) {
    if (view->length < 2) return 0;
    const uint8_t* p = view->value;
    return (int16_t)((p[0] << 8) | p[1]);
}

int64_t htlv_read_long(const htlv_view_t* view) {
    if (view->length < 8) return 0;
    const uint8_t* p = view->value;
    int64_t r = 0;
    for(int i=0; i<8; i++) {
        r = (r << 8) | p[i];
    }
    return r;
}

double htlv_read_double(const htlv_view_t* view) {
    // Java code uses ByteOrder.LITTLE_ENDIAN for Double
    if (view->length < 8) return 0.0;
    uint64_t raw = 0;
    const uint8_t* p = view->value;
    // Little Endian reconstruction
    for(int i=0; i<8; i++) {
        raw |= ((uint64_t)p[i] << (i*8));
    }
    double res;
    memcpy(&res, &raw, 8);
    return res;
}

bool htlv_read_bool(const htlv_view_t* view) {
    if (view->length == 0) return false;
    return view->value[0] == 1;
}

// --- Write Implementation ---

void htlv_writer_init(htlv_writer_t* writer, uint8_t* buffer, size_t capacity) {
    writer->buffer = buffer;
    writer->capacity = capacity;
    writer->offset = 0;
}

htlv_result_t htlv_write_tag(htlv_writer_t* writer, uint8_t tag, const uint8_t* value, size_t len) {
    // Check space: 1 (Tag) + VarInt(len) + len
    int varint_sz = get_varint_size((uint32_t)len);
    if (writer->offset + 1 + varint_sz + len > writer->capacity) {
        return HTLV_ERR_BUFFER_TOO_SMALL;
    }

    // Write Tag
    writer->buffer[writer->offset++] = tag;

    // Write Length
    size_t vi_bytes;
    write_varint(writer->buffer + writer->offset, writer->capacity - writer->offset, (uint32_t)len, &vi_bytes);
    writer->offset += vi_bytes;

    // Write Value
    if (len > 0 && value != NULL) {
        memcpy(writer->buffer + writer->offset, value, len);
        writer->offset += len;
    }

    return HTLV_OK;
}

htlv_result_t htlv_write_int(htlv_writer_t* writer, uint8_t tag, int32_t value) {
    uint8_t bytes[4];
    bytes[0] = (value >> 24) & 0xFF;
    bytes[1] = (value >> 16) & 0xFF;
    bytes[2] = (value >> 8) & 0xFF;
    bytes[3] = value & 0xFF;
    return htlv_write_tag(writer, tag, bytes, 4);
}

htlv_result_t htlv_write_short(htlv_writer_t* writer, uint8_t tag, int16_t value) {
    uint8_t bytes[2];
    bytes[0] = (value >> 8) & 0xFF;
    bytes[1] = value & 0xFF;
    return htlv_write_tag(writer, tag, bytes, 2);
}

htlv_result_t htlv_write_long(htlv_writer_t* writer, uint8_t tag, int64_t value) {
    uint8_t bytes[8];
    for (int i=7; i>=0; i--) {
        bytes[i] = value & 0xFF;
        value >>= 8;
    }
    return htlv_write_tag(writer, tag, bytes, 8);
}

htlv_result_t htlv_write_double(htlv_writer_t* writer, uint8_t tag, double value) {
    uint64_t raw;
    memcpy(&raw, &value, 8);
    uint8_t bytes[8];
    // Little Endian for Double
    for (int i=0; i<8; i++) {
        bytes[i] = (raw >> (i*8)) & 0xFF;
    }
    return htlv_write_tag(writer, tag, bytes, 8);
}

htlv_result_t htlv_write_bool(htlv_writer_t* writer, uint8_t tag, bool value) {
    uint8_t v = value ? 1 : 0;
    return htlv_write_tag(writer, tag, &v, 1);
}

htlv_result_t htlv_write_string(htlv_writer_t* writer, uint8_t tag, const char* str) {
    return htlv_write_tag(writer, tag, (const uint8_t*)str, strlen(str));
}

// Size calc helpers
size_t htlv_calc_len_tag(size_t value_len) {
    return 1 + get_varint_size((uint32_t)value_len) + value_len;
}
size_t htlv_calc_len_int() { return 1 + 1 + 4; } // Tag(1) + Len(1, since 4<128) + 4
size_t htlv_calc_len_long() { return 1 + 1 + 8; }