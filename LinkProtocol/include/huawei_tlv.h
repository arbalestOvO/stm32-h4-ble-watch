//
// Created by 19571 on 2025/12/25.
//

#ifndef TEST_PROTOCOL_HUAWEI_TLV_H
#define TEST_PROTOCOL_HUAWEI_TLV_H
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// 状态码
typedef enum {
    HTLV_OK = 0,
    HTLV_ERR_BUFFER_TOO_SMALL,
    HTLV_ERR_INVALID_VARINT,
    HTLV_ERR_MALFORMED,
    HTLV_ERR_NOT_FOUND
} htlv_result_t;

// TLV 视图 (Zero Copy的核心，只持有指针)
typedef struct {
    uint8_t tag;
    uint32_t length;     // Value的长度
    const uint8_t* value; // 指向原始Buffer中的Value起始位置
} htlv_view_t;

// 迭代器上下文
typedef struct {
    const uint8_t* buffer;
    size_t size;
    size_t offset;
    htlv_view_t current; // 当前解析到的TLV
} htlv_iter_t;

// 写入上下文
typedef struct {
    uint8_t* buffer;
    size_t capacity;
    size_t offset;
} htlv_writer_t;

// --- 基础 API ---

/**
 * 初始化迭代器
 * @param iter 迭代器指针
 * @param buffer 数据源
 * @param size 数据长度
 */
void htlv_iter_init(htlv_iter_t* iter, const uint8_t* buffer, size_t size);

/**
 * 解析下一个 TLV 条目
 * @param iter 迭代器
 * @return true 如果成功解析下一个，false 如果到达末尾或出错
 */
bool htlv_iter_next(htlv_iter_t* iter);

/**
 * 在 Buffer 中查找特定 Tag (线性搜索)
 * @return 0 if found, error code otherwise
 */
htlv_result_t htlv_find(const uint8_t* buffer, size_t size, uint8_t tag, htlv_view_t* out_view);

// --- 宏：遍历辅助 ---
// 使用示例: HTLV_FOREACH(it, buffer, len) { printf("%d", it->current.tag); }
#define HTLV_FOREACH(iter_ptr, buf, len) \
    for (htlv_iter_init(iter_ptr, buf, len); htlv_iter_next(iter_ptr); )

// --- 值读取 API (处理 Endian) ---
int32_t htlv_read_int(const htlv_view_t* view);
int16_t htlv_read_short(const htlv_view_t* view);
int64_t htlv_read_long(const htlv_view_t* view);
double htlv_read_double(const htlv_view_t* view); // Little Endian per Java code
size_t htlv_read_string(const htlv_view_t* view, char* out_buf, size_t buf_size);
bool htlv_read_bool(const htlv_view_t* view);
// String 实际上就是 (char*)view->value，因为是 UTF-8，但注意不一定以 \0 结尾

// --- 序列化 API ---

void htlv_writer_init(htlv_writer_t* writer, uint8_t* buffer, size_t capacity);

htlv_result_t htlv_write_tag(htlv_writer_t* writer, uint8_t tag, const uint8_t* value, size_t len);
htlv_result_t htlv_write_int(htlv_writer_t* writer, uint8_t tag, int32_t value);
htlv_result_t htlv_write_short(htlv_writer_t* writer, uint8_t tag, int16_t value);
htlv_result_t htlv_write_long(htlv_writer_t* writer, uint8_t tag, int64_t value);
htlv_result_t htlv_write_double(htlv_writer_t* writer, uint8_t tag, double value);
htlv_result_t htlv_write_bool(htlv_writer_t* writer, uint8_t tag, bool value);
htlv_result_t htlv_write_string(htlv_writer_t* writer, uint8_t tag, const char* str);

// 计算需要的长度 (Dry Run)
size_t htlv_calc_len_tag(size_t value_len);
size_t htlv_calc_len_int(void);
size_t htlv_calc_len_long(void);

#ifdef __cplusplus
}
#endif
#endif //TEST_PROTOCOL_HUAWEI_TLV_H