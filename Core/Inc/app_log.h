//
// Created by 19571 on 2025/12/27.
//

#ifndef ABOLUO_EXIT_APP_LOG_H
#define ABOLUO_EXIT_APP_LOG_H

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

#endif //ABOLUO_EXIT_APP_LOG_H