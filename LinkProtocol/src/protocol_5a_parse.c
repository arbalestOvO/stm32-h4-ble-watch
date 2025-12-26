//
// Created by 19571 on 2025/12/25.
//

#include "protocol_5a_parse.h"
#include <string.h>

// 公用 CRC16 算法
static uint16_t crc16_update(uint16_t crc, uint8_t data) {
    data ^= (uint8_t)(crc & 0xFF);
    data ^= data << 4;
    return ((((uint16_t)data << 8) | ((crc >> 8) & 0xFF)) ^ (uint8_t)(data >> 4) ^ ((uint16_t)data << 3));
}

// =============================================================================
//  RX SECTION: 协议解析逻辑
// =============================================================================

void parser_init(proto_parser_t *parser) {
    memset(parser, 0, sizeof(proto_parser_t));
    parser->state = STATE_WAIT_SOF;
}

static void parser_input_byte(proto_parser_t *parser, uint8_t byte, frame_recv_cb_t callback) {
    switch (parser->state) {
        case STATE_WAIT_SOF:
            if (byte == PROTO_SOF) {
                parser->state = STATE_WAIT_LEN_LOW;
                parser->calculated_crc = 0xFFFF;
                parser->calculated_crc = crc16_update(parser->calculated_crc, byte);
            }
            break;

        case STATE_WAIT_LEN_LOW:
            parser->expected_len = byte;
            parser->calculated_crc = crc16_update(parser->calculated_crc, byte);
            parser->state = STATE_WAIT_LEN_HIGH;
            break;

        case STATE_WAIT_LEN_HIGH:
            parser->expected_len |= ((uint16_t)byte << 8);
            parser->calculated_crc = crc16_update(parser->calculated_crc, byte);

            // 长度校验：如果宣称的长度超过了我们接收缓存的能力，则视为非法帧重置
            // 注意：+2 是为了包含头部中未计入length的控制字段冗余，实际业务中可根据需求放宽
            if (parser->expected_len > RX_MAX_BUFFER_SIZE + 2) {
                parser->state = STATE_WAIT_SOF;
            } else {
                parser->state = STATE_WAIT_CONTROL;
            }
            break;

        case STATE_WAIT_CONTROL:
            parser->control = byte;
            parser->calculated_crc = crc16_update(parser->calculated_crc, byte);

            // 计算 Payload 长度
            if (parser->control == CTRL_NO_FRAG) {
                parser->fsn = 0; // 不分帧无 FSN
                if (parser->expected_len < 1) { // 至少要有 Control 字段的长度
                     parser->state = STATE_WAIT_SOF; return;
                }
                parser->payload_target_len = parser->expected_len - 1; // 减去 Control(1)
                parser->state = STATE_WAIT_PAYLOAD;
            } else {
                // 分帧模式，后面跟 FSN
                parser->state = STATE_WAIT_FSN;
            }
            break;

        case STATE_WAIT_FSN:
            parser->fsn = byte;
            parser->calculated_crc = crc16_update(parser->calculated_crc, byte);

            if (parser->expected_len < 2) { // 至少要有 Control(1) + FSN(1)
                parser->state = STATE_WAIT_SOF; return;
            }
            parser->payload_target_len = parser->expected_len - 2; // 减去 Control(1) + FSN(1)
            parser->state = STATE_WAIT_PAYLOAD;
            break;

        case STATE_WAIT_PAYLOAD:
            if (parser->payload_target_len == 0) {
                // 空 Payload，直接跳过
            } else {
                parser->payload_buffer[parser->payload_index++] = byte;
                parser->calculated_crc = crc16_update(parser->calculated_crc, byte);

                if (parser->payload_index < parser->payload_target_len) {
                    return; // 继续接收 payload
                }
            }
            parser->state = STATE_WAIT_CRC_LOW;
            break;

        case STATE_WAIT_CRC_LOW:
            parser->received_crc = byte;
            parser->state = STATE_WAIT_CRC_HIGH;
            break;

        case STATE_WAIT_CRC_HIGH:
            parser->received_crc |= ((uint16_t)byte << 8);

            if (parser->calculated_crc == parser->received_crc) {
                if (callback) {
                    callback(parser->control, parser->fsn, parser->payload_buffer, parser->payload_target_len);
                }
            }
            // 无论成功失败，处理完一帧后重置
            parser_init(parser);
            break;

        default:
            parser_init(parser);
            break;
    }
}

void parser_input_buffer(proto_parser_t *parser, uint8_t *data, uint16_t len, frame_recv_cb_t callback) {
    for (uint16_t i = 0; i < len; i++) {
        parser_input_byte(parser, data[i], callback);
    }
}

// =============================================================================
//  TX SECTION: 协议组帧逻辑
// =============================================================================
uint8_t frame_buf[2048];
/**
 * @brief 内部静态函数：构建并输出单个物理帧
 * 不对外暴露，仅供 parser_send_packet 调用
 */
static void build_single_frame(uint8_t control, uint8_t fsn, const uint8_t *data, uint16_t len, frame_send_cb_t send_cb) {
    // 假设最大帧不超过 2KB，这里在栈上分配 buffer。
    // 如果嵌入式栈空间很小(如 ThreadX 线程栈 < 1K)，建议改为传入外部 buffer 或使用 static buffer
    uint16_t idx = 0;
    uint16_t crc = 0xFFFF;

    // 1. SOF
    frame_buf[idx++] = PROTO_SOF;
    crc = crc16_update(crc, PROTO_SOF);

    // 2. Length (不包含 SOF, Length本身, CRC)
    // 如果有 FSN，长度 = Control(1) + FSN(1) + payload_len
    // 如果无 FSN，长度 = Control(1) + payload_len
    uint16_t length_val = 1 + len;
    if (control != CTRL_NO_FRAG) {
        length_val += 1; // FSN
    }

    frame_buf[idx++] = length_val & 0xFF;
    crc = crc16_update(crc, frame_buf[idx-1]);

    frame_buf[idx++] = (length_val >> 8) & 0xFF;
    crc = crc16_update(crc, frame_buf[idx-1]);

    // 3. Control
    frame_buf[idx++] = control;
    crc = crc16_update(crc, control);

    // 4. FSN (Conditional)
    if (control != CTRL_NO_FRAG) {
        frame_buf[idx++] = fsn;
        crc = crc16_update(crc, fsn);
    }

    // 5. Payload
    for (uint16_t i = 0; i < len; i++) {
        frame_buf[idx++] = data[i];
        crc = crc16_update(crc, data[i]);
    }

    // 6. CRC
    frame_buf[idx++] = crc & 0xFF;
    frame_buf[idx++] = (crc >> 8) & 0xFF;

    // 发送
    if (send_cb) {
        send_cb(frame_buf, idx);
    }
}

void parser_send_packet(uint8_t *payload, uint16_t total_len, uint16_t mfs, frame_send_cb_t send_cb) {
    if (!payload || !send_cb) return;

    // 安全检查：MFS 必须能够容纳最小开销 (7字节) 加上至少 1 字节数据
    // 如果 mfs < 8，根本无法组出符合你要求的 (payload <= mfs - 7) 的有效帧
    if (mfs < PROTO_OVERHEAD_MAX + 1) {
        // MFS 设置过小，无法发送，这里直接返回或可以增加错误回调
        return;
    }

    uint16_t max_payload_per_frame = mfs - PROTO_OVERHEAD_MAX;

    // 情况 1: 数据很短，不需要分帧
    // 注意：即使是不分帧 (NO_FRAG)，我们也依然按照 max_payload_per_frame 来判断
    // 这样能保证物理帧大小绝对不超过 mfs。
    // (NO_FRAG 实际上少一个 FSN 字节，可以多发一个字节数据，但为了逻辑统一，我们从严处理)
    if (total_len <= max_payload_per_frame) {
        build_single_frame(CTRL_NO_FRAG, 0, payload, total_len, send_cb);
        return;
    }

    // 情况 2: 需要分帧
    uint16_t remaining = total_len;
    uint16_t offset = 0;
    uint8_t fsn = 0;

    // 2.1 发送起始帧 (Start Frame)
    // 强制填满第一帧的最大容量
    uint16_t chunk_len = max_payload_per_frame;

    build_single_frame(CTRL_FRAG_START, fsn++, payload + offset, chunk_len, send_cb);
    remaining -= chunk_len;
    offset += chunk_len;

    // 2.2 发送中间帧 (Middle Frames)
    // 只要剩余数据还大于一帧的容量，就继续发中间帧
    while (remaining > max_payload_per_frame) {
        build_single_frame(CTRL_FRAG_MID, fsn++, payload + offset, max_payload_per_frame, send_cb);
        remaining -= max_payload_per_frame;
        offset += max_payload_per_frame;
    }

    // 2.3 发送结束帧 (End Frame)
    // 剩下的数据全部放入结束帧
    build_single_frame(CTRL_FRAG_END, fsn++, payload + offset, remaining, send_cb);
}