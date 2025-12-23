//
// Created by 19571 on 2025/12/23.
//

#include "U3_ASS_U1.h"

#include <stdio.h>

#include "main.h"
#include "tx_api.h"

extern UART_HandleTypeDef huart1;
extern UART_HandleTypeDef huart3;

/*Variables -----------------------------------------------------------------*/
// 1. 定义线程控制块和栈
TX_THREAD u1_to_u3_thread;
TX_THREAD u3_to_u1_thread;
uint8_t u1_to_u3_stack[BRIDGE_STACK_SIZE];
uint8_t u3_to_u1_stack[BRIDGE_STACK_SIZE];

// 2. 定义消息队列控制块和缓冲区
TX_QUEUE queue_u1_to_u3;
TX_QUEUE queue_u3_to_u1;
// ThreadX 队列实际上是按“消息”计数的，这里我们将每个消息定义为 1 个 uint8_t
// 注意：ThreadX 队列底层是 ULONG 对齐的，所以这里申请空间时需要注意计算
uint8_t q_buffer_1_3[QUEUE_SIZE * sizeof(ULONG)];
uint8_t q_buffer_3_1[QUEUE_SIZE * sizeof(ULONG)];

// 3. 接收暂存变量 (用于 HAL 库的中断接收)
volatile uint8_t rx_byte_u1;
volatile uint8_t rx_byte_u3;

/*Function Prototypes -------------------------------------------------------*/
void Thread_U1_To_U3_Entry(ULONG thread_input);
void Thread_U3_To_U1_Entry(ULONG thread_input);

/**
  * @brief  初始化串口透传的 RTOS 资源
  * @note   请在 tx_application_define 中调用，或者在内核启动前调用
  */
void App_UART_Bridge_Init(void)
{
    // 1. 创建从 UART1 到 UART3 的队列
    // TX_1_ULONG 表示每个消息的大小为 1 个 32位字 (虽然我们只传 uint8，但最小单位是 ULONG)
    tx_queue_create(&queue_u1_to_u3, "Queue U1->U3", TX_1_ULONG,
                    q_buffer_1_3, sizeof(q_buffer_1_3));

    // 2. 创建从 UART3 到 UART1 的队列
    tx_queue_create(&queue_u3_to_u1, "Queue U3->U1", TX_1_ULONG,
                    q_buffer_3_1, sizeof(q_buffer_3_1));

    // 3. 创建处理线程 1 (优先级设为中等，例如 10)
    tx_thread_create(&u1_to_u3_thread, "Thread U1->U3",
                     Thread_U1_To_U3_Entry, 0,
                     u1_to_u3_stack, BRIDGE_STACK_SIZE,
                     10, 10, TX_NO_TIME_SLICE, TX_AUTO_START);

    // 4. 创建处理线程 2
    tx_thread_create(&u3_to_u1_thread, "Thread U3->U1",
                     Thread_U3_To_U1_Entry, 0,
                     u3_to_u1_stack, BRIDGE_STACK_SIZE,
                     10, 10, TX_NO_TIME_SLICE, TX_AUTO_START);
}

/**
  * @brief  启动接收中断 (需要在主循环或线程启动初期调用一次)
  */
void App_UART_Start_Receiving(void)
{
    HAL_UART_Receive_IT(&huart1, (uint8_t *)&rx_byte_u1, 1);
    HAL_UART_Receive_IT(&huart3, (uint8_t *)&rx_byte_u3, 1);
}

/**
  * @brief  Thread: 读取队列并发送到 UART3
  */
void Thread_U1_To_U3_Entry(ULONG thread_input)
{
    ULONG received_msg;
    uint8_t data_to_send;

    // 确保第一次接收已启动
    App_UART_Start_Receiving();

    while(1)
    {
        // 挂起等待队列中有数据 (TX_WAIT_FOREVER)
        // 这一步是高效的关键：没有数据时，线程不占用任何 CPU
        if (tx_queue_receive(&queue_u1_to_u3, &received_msg, TX_WAIT_FOREVER) == TX_SUCCESS)
        {
            data_to_send = (uint8_t)(received_msg & 0xFF);

            // 阻塞发送，超时设为 100ms
            // 在 RTOS 线程中阻塞是可以的，它只会挂起当前线程，让出 CPU 给其他线程
            HAL_UART_Transmit(&huart3, &data_to_send, 1, 100);
        }
    }
}

/**
  * @brief  Thread: 读取队列并发送到 UART1
  */
void Thread_U3_To_U1_Entry(ULONG thread_input)
{
    ULONG received_msg;
    uint8_t data_to_send;

    while(1)
    {
        if (tx_queue_receive(&queue_u3_to_u1, &received_msg, TX_WAIT_FOREVER) == TX_SUCCESS)
        {
            data_to_send = (uint8_t)(received_msg & 0xFF);
            HAL_UART_Transmit(&huart1, &data_to_send, 1, 100);
        }
    }
}

/**
  * @brief  HAL UART 接收完成回调
  * @note   这是中断上下文，必须尽可能快地执行
  */
void HAL_UART_RxCpltCallback(UART_HandleTypeDef *huart)
{
    ULONG msg_to_send;

    if (huart->Instance == USART1)
    {
        msg_to_send = (ULONG)rx_byte_u1;

        // 将数据发送到队列，使用 TX_NO_WAIT 因为我们在中断里
        // 如果队列满了，这里会丢弃数据（表明处理线程太慢或波特率不匹配）
        tx_queue_send(&queue_u1_to_u3, &msg_to_send, TX_NO_WAIT);

        // 重新开启中断接收
        HAL_UART_Receive_IT(&huart1, (uint8_t *)&rx_byte_u1, 1);
    }
    else if (huart->Instance == USART3)
    {
        msg_to_send = (ULONG)rx_byte_u3;

        tx_queue_send(&queue_u3_to_u1, &msg_to_send, TX_NO_WAIT);

        HAL_UART_Receive_IT(&huart3, (uint8_t *)&rx_byte_u3, 1);
    }
}