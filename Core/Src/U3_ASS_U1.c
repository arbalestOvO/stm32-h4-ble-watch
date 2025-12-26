//
// Created by 19571 on 2025/12/23.
// Modified for DMA Circular Mode + IDLE Detection
//

#include "U3_ASS_U1.h"
#include <stdio.h>
#include <string.h> // For memcpy
#include "main.h"
#include "tx_api.h"

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart3;

/* Configuration ------------------------------------------------------------*/
#define DMA_RX_BUF_SIZE     256  // 根据实际数据量调整，建议 256, 512, 1024
#define UART_TX_TIMEOUT     100  // ms

/* Variables ----------------------------------------------------------------*/
// 1. 线程控制块
TX_THREAD u1_to_u3_thread;
TX_THREAD u3_to_u1_thread;
uint8_t u1_to_u3_stack[BRIDGE_STACK_SIZE];
uint8_t u3_to_u1_stack[BRIDGE_STACK_SIZE];

// 3. 事件标志组 (用于中断通知线程有数据到来)
TX_EVENT_FLAGS_GROUP uart_event_flags;
#define EVT_FLAG_U1_RX   (1 << 0)
#define EVT_FLAG_U3_RX   (1 << 1)

// 4. DMA 接收相关变量
// 接收缓冲区 (DMA 会循环写入这里)
uint8_t rx_buf_u1[DMA_RX_BUF_SIZE] __attribute__((aligned(4)));
uint8_t rx_buf_u3[DMA_RX_BUF_SIZE] __attribute__((aligned(4)));

// 读取指针 (记录应用层读到了哪里的位置)
volatile uint16_t u1_rx_read_pos = 0;
volatile uint16_t u3_rx_read_pos = 0;

/* Function Prototypes ------------------------------------------------------*/
void Thread_U1_To_U3_Entry(ULONG thread_input);
void Thread_U3_To_U1_Entry(ULONG thread_input);
void Process_DMA_Buffer(UART_HandleTypeDef *huart, uint8_t *buffer, uint16_t buf_size, volatile uint16_t *read_pos, uint8_t is_u3);

/**
  * @brief  初始化串口透传的 RTOS 资源及 DMA
  */
void App_UART_Bridge_Init(void)
{
    // 1. 创建事件标志组
    tx_event_flags_create(&uart_event_flags, "UART RX Events");

    // 3. 创建处理线程
    tx_thread_create(&u1_to_u3_thread, "Thread U1->U3",
                     Thread_U1_To_U3_Entry, 0,
                     u1_to_u3_stack, BRIDGE_STACK_SIZE,
                     10, 10, TX_NO_TIME_SLICE, TX_AUTO_START);

    tx_thread_create(&u3_to_u1_thread, "Thread U3->U1",
                     Thread_U3_To_U1_Entry, 0,
                     u3_to_u1_stack, BRIDGE_STACK_SIZE,
                     10, 10, TX_NO_TIME_SLICE, TX_AUTO_START);

    // 4. 启动 DMA 接收 (Ex版本支持 ReceiveToIdle，如果不适用请换回标准 DMA Start)
    // 即使没有数据，DMA 也会挂起等待
    App_UART_Start_Receiving();
}

/**
  * @brief  启动 DMA 接收 (Circular Mode)
  * @note   需要在 CubeMX 中开启 UART1_RX 和 UART3_RX 的 DMA，并设为 Circular 模式
  */
void App_UART_Start_Receiving(void)
{
    // 这里的接收是 Circular 模式，一旦启动，除非出错，否则不需要再次调用
    // 使用 ReceiveToIdle_DMA 可以同时启用 IDLE 中断和 DMA
    HAL_UARTEx_ReceiveToIdle_DMA(&huart1, rx_buf_u1, DMA_RX_BUF_SIZE);
    HAL_UARTEx_ReceiveToIdle_DMA(&huart3, rx_buf_u3, DMA_RX_BUF_SIZE);
}

/**
  * @brief  Thread: 处理 U1 接收到的数据 -> 发送给 U3
  */
void Thread_U1_To_U3_Entry(ULONG thread_input)
{
    ULONG actual_flags;

    while(1)
    {
        // 等待 U1 接收事件 (由中断触发)
        if (tx_event_flags_get(&uart_event_flags, EVT_FLAG_U1_RX, TX_OR_CLEAR, &actual_flags, TX_WAIT_FOREVER) == TX_SUCCESS)
        {
            Process_DMA_Buffer(&huart1, rx_buf_u1, DMA_RX_BUF_SIZE, &u1_rx_read_pos, 0);
        }
    }
}

/**
  * @brief  Thread: 处理 U3 接收到的数据 -> 发送给 U1 & 推送 Queue
  */
void Thread_U3_To_U1_Entry(ULONG thread_input)
{
    ULONG actual_flags;

    while(1)
    {
        // 等待 U3 接收事件
        if (tx_event_flags_get(&uart_event_flags, EVT_FLAG_U3_RX, TX_OR_CLEAR, &actual_flags, TX_WAIT_FOREVER) == TX_SUCCESS)
        {
            Process_DMA_Buffer(&huart3, rx_buf_u3, DMA_RX_BUF_SIZE, &u3_rx_read_pos, 1);
        }
    }
}

/**
  * @brief  处理环形缓冲区的数据
  * @param  huart: 串口句柄 (用于获取当前 DMA 写入位置)
  * @param  buffer: 环形缓冲区指针
  * @param  buf_size: 缓冲区总大小
  * @param  read_pos: 上次处理到的位置指针
  * @param  is_u3: 标记是否是 U3 (如果是 U3，需要额外发给 Queue)
  */
void Process_DMA_Buffer(UART_HandleTypeDef *huart, uint8_t *buffer, uint16_t buf_size, volatile uint16_t *read_pos, uint8_t is_u3)
{
    uint16_t write_pos;
    uint16_t len;
    uint16_t current_read = *read_pos;

    // 计算当前 DMA 写到了哪里
    // 方法：总大小 - DMA剩余数据量 (CNDTR)
    write_pos = buf_size - __HAL_DMA_GET_COUNTER(huart->hdmarx);

    if (current_read == write_pos)
    {
        return; // 没有新数据
    }

    // 此时有两种情况：
    // 1. write > read: 数据连续 [read ... write]
    // 2. write < read: 数据回绕 [read ... end] + [0 ... write]

    if (write_pos > current_read)
    {
        len = write_pos - current_read;

        // 1. 转发处理 (Bridge)
        if (is_u3) {
            HAL_UART_Transmit(&huart1, &buffer[current_read], len, UART_TX_TIMEOUT);

        } else {
            // U1 -> U3
            HAL_UART_Transmit(&huart3, &buffer[current_read], len, UART_TX_TIMEOUT);
        }

        current_read += len;
    }
    else // 回绕情况
    {
        // 第一段：Read -> End
        uint16_t len1 = buf_size - current_read;
        if (len1 > 0)
        {
            if (is_u3) {
                HAL_UART_Transmit(&huart1, &buffer[current_read], len1, UART_TX_TIMEOUT);

            } else {
                HAL_UART_Transmit(&huart3, &buffer[current_read], len1, UART_TX_TIMEOUT);
            }
        }

        // 第二段：0 -> Write
        uint16_t len2 = write_pos;
        if (len2 > 0)
        {
            if (is_u3) {
                HAL_UART_Transmit(&huart1, &buffer[0], len2, UART_TX_TIMEOUT);

            } else {
                HAL_UART_Transmit(&huart3, &buffer[0], len2, UART_TX_TIMEOUT);
            }
        }

        current_read = write_pos;
    }

    // 更新全局读指针
    *read_pos = current_read;
}

/**
  * @brief  HAL 扩展回调: 接收事件回调 (IDLE, Half Transfer, Full Transfer 都会触发)
  * @note   这比 RxCpltCallback 更适合 DMA 变长数据接收
  */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    // 注意：Size 参数在 Circular 模式下的含义可能因 HAL 版本而异
    // 但我们主要利用这个中断来唤醒线程，具体的读写位置由 Process_DMA_Buffer 里的 CNDTR 计算

    if (huart->Instance == USART1)
    {
        tx_event_flags_set(&uart_event_flags, EVT_FLAG_U1_RX, TX_OR);
    }
    else if (huart->Instance == USART3)
    {
        tx_event_flags_set(&uart_event_flags, EVT_FLAG_U3_RX, TX_OR);
    }
}

/**
  * @brief  错误回调 (可选，用于处理 Overrun 等)
  */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART1)
    {
        // 可以在此添加错误恢复代码，例如重启 DMA
         HAL_UARTEx_ReceiveToIdle_DMA(&huart1, rx_buf_u1, DMA_RX_BUF_SIZE);
    }
    else if (huart->Instance == USART3)
    {
         HAL_UARTEx_ReceiveToIdle_DMA(&huart3, rx_buf_u3, DMA_RX_BUF_SIZE);
    }
}