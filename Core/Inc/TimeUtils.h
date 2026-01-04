#ifndef __TIME_UTILS_H
#define __TIME_UTILS_H

#include "stm32h7xx_hal.h"
#include <time.h>
#include <stdio.h>

// 定义魔术字，用于判断RTC是否已经初始化过
// 存放在 RTC_BKP_DR0 中
#define RTC_BKP_MAGIC_NUMBER  0x32F2

// === 新增辅助宏：用于在 2025 和 25 之间转换 ===
// 使用方法: time.year = RTC_FORMAT_YEAR(2025); // 结果为 25
#define RTC_FORMAT_YEAR(y)      ((uint8_t)((y) >= 2000 ? (y) - 2000 : (y)))
// 使用方法: uint16_t full_year = RTC_FULL_YEAR(time.year); // 结果为 2025
#define RTC_FULL_YEAR(y)        ((uint16_t)((y) + 2000))

// 定义时间结构体，方便应用层使用 (解耦 HAL 库)
typedef struct {
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
    uint8_t date;   // 日
    uint8_t month;  // 月
    uint8_t year;   // 年 (0-99). 硬件限制仅存储后两位 (例如: 填 25 代表 2025年)
    uint8_t week;   // 星期 (1 = Monday, ..., 7 = Sunday)
} SystemTime_t;

// 核心功能函数
// void Time_Init(void); // 已废弃：初始化逻辑已移交至 main.c 中的 MX_RTC_Init
void Time_SetCalendar(SystemTime_t *time);
void Time_GetCalendar(SystemTime_t *time);

// 辅助转换函数
time_t Time_GetUnixTimestamp(void);
void Time_SetFromUnixTimestamp(time_t timestamp);

// 格式化打印 (调试用)
void Time_PrintDebug(void);

#endif // __TIME_UTILS_H