//
// Created by 19571 on 2025/12/25.
//

#ifndef ABOLUO_EXIT_PROTOCOL_5A_H
#define ABOLUO_EXIT_PROTOCOL_5A_H

#include <stdint.h>
#include <stdbool.h>

// -----------------------------------------------------------------------------
// 协议常量与定义
// -----------------------------------------------------------------------------
#define PROTO_SOF           0x5A
#define PROTO_OVERHEAD_MAX  7     // SOF(1) + Len(2) + Ctrl(1) + FSN(1) + CRC(2)
#define PROTO_HEADER_MIN    4     // SOF(1) + Len(2) + Ctrl(1) (无FSN情况)

// 控制帧类型
#define CTRL_NO_FRAG        0x00
#define CTRL_FRAG_START     0x01
#define CTRL_FRAG_MID       0x02
#define CTRL_FRAG_END       0x03

// -----------------------------------------------------------------------------
// 接收解析相关 (RX)
// -----------------------------------------------------------------------------

// 解析器状态枚举
typedef enum {
    STATE_WAIT_SOF,
    STATE_WAIT_LEN_LOW,
    STATE_WAIT_LEN_HIGH,
    STATE_WAIT_CONTROL,
    STATE_WAIT_FSN,
    STATE_WAIT_PAYLOAD,
    STATE_WAIT_CRC_LOW,
    STATE_WAIT_CRC_HIGH
} parse_state_t;

// 解析器上下文结构体
typedef struct {
    parse_state_t state;

    // 临时存储协议字段
    uint16_t expected_len;    // 从Length字段解析出的长度
    uint8_t  control;
    uint8_t  fsn;

    // 接收Payload缓存
    // 注意：这里定义的是接收的最大缓存，如果你希望动态分配，可以改为指针
    #define RX_MAX_BUFFER_SIZE 1024
    uint8_t  payload_buffer[RX_MAX_BUFFER_SIZE];
    uint16_t payload_index;
    uint16_t payload_target_len;

    // CRC 计算
    uint16_t calculated_crc;
    uint16_t received_crc;

} proto_parser_t;

// 接收回调：当解析出一个完整逻辑帧（无论是否分片）的片段时调用
// 业务层需要根据 control 和 fsn 来决定是组装还是直接使用
typedef void (*frame_recv_cb_t)(uint8_t control, uint8_t fsn, uint8_t *data, uint16_t len);

// 初始化解析器
void parser_init(proto_parser_t *parser);
// 输入数据流进行解析
void parser_input_buffer(proto_parser_t *parser, uint8_t *data, uint16_t len, frame_recv_cb_t callback);


// -----------------------------------------------------------------------------
// 发送组帧相关 (TX)
// -----------------------------------------------------------------------------

// 发送回调：当组装好一个物理帧时调用，用户应在此将数据写入硬件
// frame_data: 完整的物理帧数据
// frame_len: 物理帧长度
typedef void (*frame_send_cb_t)(uint8_t *frame_data, uint16_t frame_len);

/**
 * @brief 协议组帧发送函数
 * * 根据传入的 MFS (Maximum Frame Size) 自动将 payload 切分为多个帧并通过回调发出。
 * * @param payload   要发送的应用层数据（如需加密，请先加密再传入）
 * @param total_len 数据总长度
 * @param mfs       链路层最大帧大小 (MTU)，必须 >= 8 (至少容纳1字节payload + 7字节开销)
 * @param send_cb   硬件发送回调函数
 */
void parser_send_packet(uint8_t *payload, uint16_t total_len, uint16_t mfs, frame_send_cb_t send_cb);

#endif //ABOLUO_EXIT_PROTOCOL_5A_H