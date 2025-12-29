//
// Created by 19571 on 2025/12/27.
//

#ifndef ABOLUO_EXIT_APP_LOG_H
#define ABOLUO_EXIT_APP_LOG_H

#include <stddef.h>
#include <stdint.h>

/**
 * @brief  初始化打印任务 (在 tx_application_define 中调用)
 */
void Printf_Init(void);

/**
 * @brief  写入数据到缓冲区 (供 printf 重定向调用)
 * @param  data: 数据指针
 * @param  len:  数据长度
 */
void UART_Buffer_Write(uint8_t *data, uint16_t len);

void print_current_thread_stack_info(void);


/**
 * @brief 以16进制格式打印内存数据
 * * @param tag  标签（用于识别打印输出的内容）
 * @param data 数据指针
 * @param len  数据长度
 */
void print_hex(const char *tag, const uint8_t *data, size_t len);
#endif //ABOLUO_EXIT_APP_LOG_H