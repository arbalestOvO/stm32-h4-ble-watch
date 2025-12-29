#ifndef HICHAIN_UTILS_H
#define HICHAIN_UTILS_H

#include <stddef.h>
#include <stdint.h>

/**
 * 将字节数组转换为十六进制字符串 (大写)
 * @param src 源字节数组
 * @param len 数组长度
 * @return 新分配的字符串，需要调用者 free()，如果失败返回 NULL
 */
char* bytes_to_hex(const uint8_t *src, size_t len);

/**
 * 将十六进制字符串转换为字节数组
 * @param hex 十六进制字符串
 * @param out_len [输出] 转换后的字节长度
 * @return 新分配的字节数组，需要调用者 free()，如果失败返回 NULL
 */
uint8_t* hex_string_to_bytes(const char *hex, size_t *out_len);

#endif // HICHAIN_UTILS_H