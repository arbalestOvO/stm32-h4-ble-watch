#include "TimeUtils.h"

extern RTC_HandleTypeDef hrtc;

/* * 内部函数声明
 */
static void Error_Handler_RTC(void);

// 注意：Time_Init 函数已删除。
// 初始化逻辑请参考 main.c 中的 MX_RTC_Init 函数，
// 那里包含了掉电保持(Backup Register)的检查逻辑。

/**
  * @brief  设置当前的日历时间
  */
void Time_SetCalendar(SystemTime_t *time)
{
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    // 1. 填充时间结构体
    sTime.Hours = time->hours;
    sTime.Minutes = time->minutes;
    sTime.Seconds = time->seconds;
    sTime.DayLightSaving = RTC_DAYLIGHTSAVING_NONE;
    sTime.StoreOperation = RTC_STOREOPERATION_RESET;

    // 2. 填充日期结构体
    sDate.WeekDay = time->week;
    sDate.Month = time->month;
    sDate.Date = time->date;

    // 【年份处理】
    // 尽管头文件建议传入 0-99，但为了防止用户强制传入如 (uint8_t)2025 = 233 导致错误，
    // 这里增加一个取模保护，确保写入 RTC 的年份永远在 0-99 范围内。
    // 如果用户修改了 SystemTime_t 的 year 为 uint16_t 并传入 2025，这里也会自动转为 25。
    sDate.Year = time->year % 100;

    // 3. 写入 RTC
    if (HAL_RTC_SetTime(&hrtc, &sTime, RTC_FORMAT_BIN) != HAL_OK)
    {
        Error_Handler_RTC();
    }

    if (HAL_RTC_SetDate(&hrtc, &sDate, RTC_FORMAT_BIN) != HAL_OK)
    {
        Error_Handler_RTC();
    }

    // 4. 写入魔术字到备份寄存器
    // 即使在运行中重新设置时间，也确保魔术字存在
    HAL_RTCEx_BKUPWrite(&hrtc, RTC_BKP_DR0, RTC_BKP_MAGIC_NUMBER);
}

/**
  * @brief  读取当前的日历时间
  */
void Time_GetCalendar(SystemTime_t *time)
{
    RTC_TimeTypeDef sTime = {0};
    RTC_DateTypeDef sDate = {0};

    // 1. 读取时间
    // 注意：必须先读 Time 再读 Date，这是为了解锁影子寄存器
    HAL_RTC_GetTime(&hrtc, &sTime, RTC_FORMAT_BIN);

    // 2. 读取日期
    HAL_RTC_GetDate(&hrtc, &sDate, RTC_FORMAT_BIN);

    // 3. 转换到输出结构体
    time->hours = sTime.Hours;
    time->minutes = sTime.Minutes;
    time->seconds = sTime.Seconds;

    time->year = sDate.Year;
    time->month = sDate.Month;
    time->date = sDate.Date;
    time->week = sDate.WeekDay;
}

/**
  * @brief  获取 Unix 时间戳
  */
time_t Time_GetUnixTimestamp(void)
{
    SystemTime_t currTime;
    struct tm time_struct;

    Time_GetCalendar(&currTime);

    // tm_year 是从 1900 开始的
    // RTC year 是 0-99 (代表 2000-2099)
    // 例子: 2025年 -> RTC year=25 -> 2000+25 = 2025 -> 2025-1900 = 125
    time_struct.tm_year = currTime.year + 125; // 暂时，应该要写100的

    time_struct.tm_mon  = currTime.month - 1;  // 0-11
    time_struct.tm_mday = currTime.date;
    time_struct.tm_hour = currTime.hours;
    time_struct.tm_min  = currTime.minutes;
    time_struct.tm_sec  = currTime.seconds;
    time_struct.tm_isdst = -1;

    return mktime(&time_struct) * 1000;
}

/**
  * @brief  从 Unix 时间戳设置 RTC
  */
void Time_SetFromUnixTimestamp(time_t timestamp)
{
    struct tm *time_struct;
    SystemTime_t sysTime;

    time_struct = localtime(&timestamp);

    // tm_year: years since 1900 (e.g., 125 for 2025)
    // RTC year: years since 2000 (e.g., 25 for 2025)
    // 125 - 100 = 25
    sysTime.year = time_struct->tm_year - 100;

    sysTime.month = time_struct->tm_mon + 1;
    sysTime.date = time_struct->tm_mday;
    sysTime.hours = time_struct->tm_hour;
    sysTime.minutes = time_struct->tm_min;
    sysTime.seconds = time_struct->tm_sec;

    if (time_struct->tm_wday == 0) sysTime.week = 7;
    else sysTime.week = time_struct->tm_wday;

    Time_SetCalendar(&sysTime);
}

void Time_PrintDebug(void)
{
    SystemTime_t t;
    Time_GetCalendar(&t);
    // 打印完整年份: 20xx
    printf("RTC: 20%02d-%02d-%02d %02d:%02d:%02d (Week: %d)\n", 
           t.year, t.month, t.date, t.hours, t.minutes, t.seconds, t.week);
}

// 错误处理
static void Error_Handler_RTC(void)
{
    while(1) {}
}