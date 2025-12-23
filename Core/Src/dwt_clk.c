//
// Created by 19571 on 2025/12/23.
//

#include "dwt_clk.h"
#include "stm32h7xx.h" // 确保包含芯片头文件

/* 静态变量，记录每微秒的 tick 数 */
static uint32_t fac_us = 0;
/**
  * @brief  初始化 DWT 延时
  * @note   必须在 main() 开始时，时钟配置完成后调用一次
  */
void DWT_Delay_Init(void)
{
    /* 1. 确保 CoreDebug 的跟踪功能被使能 */
    CoreDebug->DEMCR |= CoreDebug_DEMCR_TRCENA_Msk;

    /* 2. 复位周期计数器 (可选，但推荐) */
    DWT->CYCCNT = 0;

    /* 3. 使能 DWT 周期计数器 */
    DWT->CTRL |= DWT_CTRL_CYCCNTENA_Msk;

    /* 4. 计算每微秒需要的 cycle 数 */
    /* SystemCoreClock 保存了当前 CPU 频率 (例如 480000000) */
    fac_us = SystemCoreClock / 1000000;
}

/**
  * @brief  微秒级延时
  * @param  us: 延时微秒数
  */
void delay_us(uint32_t us)
{
    uint32_t start_tick = DWT->CYCCNT;
    uint32_t target_ticks = us * fac_us;

    /* 使用无符号算术减法自动处理溢出问题 (32位回绕) */
    while ((DWT->CYCCNT - start_tick) < target_ticks)
    {
        /* 等待期间什么都不做 */
        /* 如果是在极低功耗模式下，可能需要加 __NOP(); */
    }
}
