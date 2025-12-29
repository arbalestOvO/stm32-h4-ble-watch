//
// Created by 19571 on 2025/12/27.
//

#include "app_log.h"

#include "app_log.h"

#include <stdio.h>

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

void print_current_thread_stack_info(void)
{
    TX_THREAD *current_thread;
    CHAR *name;
    UINT state;
    ULONG run_count;
    UINT priority;
    UINT preemption_threshold;
    ULONG time_slice;
    TX_THREAD *next_thread;
    TX_THREAD *suspended_thread;

    // 1. 获取当前线程指针
    current_thread = tx_thread_identify();

    if (current_thread == NULL) {
        printf("Current context is ISR or Initialization, not a thread.\r\n");
        return;
    }

    // 2. 获取线程基础信息
    // 注意：stack_start 是栈底（低地址），stack_end 是栈顶（高地址，增长方向通常向下）
    // stack_ptr 是当前的栈指针位置
    void *stack_start;
    void *stack_end;
    void *stack_ptr; // 当前栈指针

    tx_thread_info_get(current_thread, &name, &state, &run_count, &priority,
                       &preemption_threshold, &time_slice, &next_thread,
                       &suspended_thread);

    // 直接访问 TCB 结构体获取栈边界（这也是官方API内部的做法）
    // 注意：直接访问结构体成员比调API更直接，但依赖版本兼容性
    stack_start = current_thread -> tx_thread_stack_start;
    stack_end   = current_thread -> tx_thread_stack_end;
    stack_ptr   = current_thread -> tx_thread_stack_ptr;

    // 3. 计算大小
    ULONG total_size = (ULONG)((CHAR*)stack_end - (CHAR*)stack_start + 1);
    ULONG current_free = (ULONG)((CHAR*)stack_ptr - (CHAR*)stack_start);

    printf("--- Thread: %s ---\r\n", name);
    printf("Total Stack Size: %lu bytes\r\n", total_size);
    printf("Current Free:     %lu bytes\r\n", current_free);

    // 4. 计算历史最小剩余 (需要 TX_ENABLE_STACK_CHECKING)
#ifdef TX_ENABLE_STACK_CHECKING
    // 检测栈中是否还有 0xEF 模式来确定从未被接触过的内存
    // 注意：tx_thread_stack_analyze 不是所有标准分发包都有，如果没有，可以使用下面的逻辑

    ULONG *check_ptr = (ULONG *)stack_start;
    while (check_ptr < (ULONG *)stack_end) {
        if (*check_ptr != 0xEFEFEFEFUL) {
            break;
        }
        check_ptr++;
    }

    ULONG min_ever_free = (ULONG)((CHAR*)check_ptr - (CHAR*)stack_start);
    printf("Min Ever Free:    %lu bytes (High Water Mark)\r\n", min_ever_free);

    if (min_ever_free < 100) {
         printf("WARNING: Stack nearly overflowed!\r\n");
    }
#else
    printf("Min Ever Free:    Unknown (Enable TX_ENABLE_STACK_CHECKING)\r\n");
#endif
    printf("------------------------\r\n");
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