//
// Created by 19571 on 2025/12/27.
//

#include <stdio.h>
#include "../Inc/app_log.h"

// ============================================================================
// 1. GCC 编译器 (CLion, STM32CubeIDE, VSCode)
// ============================================================================
#if defined(__GNUC__)

int _write(int file, char *ptr, int len)
{
    UART_Buffer_Write((uint8_t *)ptr, len);
    return len;
}

// ============================================================================
// 2. Keil MDK (ARMCC / ARMCLANG)
// ============================================================================
#elif defined(__CC_ARM) || defined(__ARMCC_VERSION)

#if !defined(MICROLIB)
#pragma import(__use_no_semihosting)
struct __FILE { int handle; };
FILE __stdout;
void _sys_exit(int x) { x = x; }
#endif

int fputc(int ch, FILE *f)
{
    uint8_t temp = (uint8_t)ch;
    UART_Buffer_Write(&temp, 1);
    return ch;
}

// ============================================================================
// 3. IAR EWARM
// ============================================================================
#elif defined(__ICCARM__)

size_t __write(int handle, const unsigned char * buffer, size_t size)
{
    UART_Buffer_Write((uint8_t *)buffer, size);
    return size;
}

#endif