//
// Created by 19571 on 2025/12/27.
//

#include "app_log.h"

#include "app_log.h"
#include "main.h"
#include "usart.h"
#include "tx_api.h"

// 引用外部句柄
extern UART_HandleTypeDef huart1;

// ============================================================================
//  配置区域
// ============================================================================
#define TX_BUFFER_SIZE  2048        // 缓冲区大小
#define LOG_THREAD_STACK_SIZE 1024  // 打印任务的堆栈大小
#define LOG_THREAD_PRIO 8          // 打印任务优先级

// ============================================================================
//  全局变量定义 (内部状态)
// ============================================================================
typedef struct {
    uint8_t buffer[TX_BUFFER_SIZE];
    volatile uint16_t head; // 写入位置
    volatile uint16_t tail; // 读取位置
} RingBuffer_t;

// 仅在当前文件可见
static RingBuffer_t g_uart_tx_buf = { .head = 0, .tail = 0 };

// ThreadX 任务资源
static TX_THREAD g_log_thread;
static uint8_t g_log_thread_stack[LOG_THREAD_STACK_SIZE];

// ============================================================================
//  函数实现
// ============================================================================

/**
 * @brief  将数据写入环形缓冲区 (生产者)
 * @note   移除了 static，供 printf_redirect.c 调用
 */
void UART_Buffer_Write(uint8_t *data, uint16_t len)
{
    // 进入临界区，防止多任务或中断打断写入
    UINT old_posture = tx_interrupt_control(TX_INT_DISABLE);

    for (uint16_t i = 0; i < len; i++)
    {
        uint16_t next_head = (g_uart_tx_buf.head + 1) % TX_BUFFER_SIZE;

        if (next_head != g_uart_tx_buf.tail)
        {
            g_uart_tx_buf.buffer[g_uart_tx_buf.head] = data[i];
            g_uart_tx_buf.head = next_head;
        }
        else
        {
            // 缓冲区满，丢弃数据
            break;
        }
    }

    tx_interrupt_control(old_posture);
}

/**
 * @brief  日志处理核心逻辑 (消费者)
 */
static uint8_t App_Log_Process(void)
{
    if (g_uart_tx_buf.head != g_uart_tx_buf.tail)
    {
        uint8_t ch = g_uart_tx_buf.buffer[g_uart_tx_buf.tail];

        // 阻塞发送1个字节，超时设为2ms
        HAL_UART_Transmit(&huart1, &ch, 1, 2);

        g_uart_tx_buf.tail = (g_uart_tx_buf.tail + 1) % TX_BUFFER_SIZE;
        return 1; // 忙碌
    }
    return 0; // 空闲
}

/**
 * @brief  打印任务入口函数
 */
static void Log_Thread_Entry(ULONG input)
{
    (void)input;

    while(1)
    {
        // 尝试处理数据
        if (App_Log_Process() == 0)
        {
            // 如果缓冲区为空，挂起任务 1 个 Tick
            tx_thread_sleep(1);
        }
    }
}

/**
 * @brief  [对外接口] 初始化并启动打印任务
 */
void Printf_Init(void)
{
    // 1. 复位缓冲区
    g_uart_tx_buf.head = 0;
    g_uart_tx_buf.tail = 0;

    // 2. 创建并自动启动线程
    tx_thread_create(&g_log_thread,
                     "Log Print Thread",
                     Log_Thread_Entry,
                     0,
                     g_log_thread_stack,
                     LOG_THREAD_STACK_SIZE,
                     LOG_THREAD_PRIO,
                     LOG_THREAD_PRIO,
                     TX_NO_TIME_SLICE,
                     TX_AUTO_START);
}