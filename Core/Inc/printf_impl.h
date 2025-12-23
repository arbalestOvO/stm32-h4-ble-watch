//
// Created by 19571 on 2025/12/20.
//

#ifndef ABOLUO_TEMPLATE_PRINTF_IMPL_H
#define ABOLUO_TEMPLATE_PRINTF_IMPL_H


#include <stdio.h>
#include "main.h"
#include "usart.h"

extern UART_HandleTypeDef huart1;

// ============================================================================
// 1. GCC 编译器 (CLion, STM32CubeIDE, VSCode)
// ============================================================================
#if defined(__GNUC__)

/* 对于 GCC/CLion，我们需要重写 _write 函数
 * 这是 libc 中 printf 调用的底层 IO 接口
 */
int _write(int file, char *ptr, int len)
{
    // 将数据通过串口1发送
    HAL_UART_Transmit(&huart1, (uint8_t *)ptr, len, HAL_MAX_DELAY);
    return len;
}

// ============================================================================
// 2. Keil MDK (ARMCC / ARMCLANG)
// ============================================================================
#elif defined(__CC_ARM) || defined(__ARMCC_VERSION)

/* 对于 Keil MDK，我们需要重写 fputc
 * 并为了避免半主机模式(Semihosting)导致程序卡死，通常需要以下配置
 * 注意：在 Keil 项目设置中勾选 "Use MicroLib" 是最简单的做法
 */

/* 只有不使用 MicroLib 时才需要下面的结构体定义来禁用半主机 */
#if !defined(MICROLIB)
#pragma import(__use_no_semihosting)
struct __FILE
{
    int handle;
};
FILE __stdout;

void _sys_exit(int x)
{
    x = x;
}
#endif

/* Keil 的 printf 底层调用 fputc */
int fputc(int ch, FILE *f)
{
    // 发送一个字符
    HAL_UART_Transmit(&huart1, (uint8_t *)&ch, 1, HAL_MAX_DELAY);
    return ch;
}

// ============================================================================
// 3. IAR EWARM (可选，为了完整性)
// ============================================================================
#elif defined(__ICCARM__)

size_t __write(int handle, const unsigned char * buffer, size_t size)
{
    HAL_UART_Transmit(&huart1, (uint8_t *)buffer, size, HAL_MAX_DELAY);
    return size;
}

#endif

#endif //ABOLUO_TEMPLATE_PRINTF_IMPL_H