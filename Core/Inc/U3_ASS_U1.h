//
// Created by 19571 on 2025/12/23.
//

#ifndef ABOLUO_EXIT_U3_ASS_U1_H
#define ABOLUO_EXIT_U3_ASS_U1_H

/* Defines ------------------------------------------------------------------*/
#define BRIDGE_STACK_SIZE   1024
#define QUEUE_SIZE          128  // 原有的队列大小定义

/* Function Prototypes ------------------------------------------------------*/
void App_UART_Bridge_Init(void);
void App_UART_Start_Receiving(void);

#endif //ABOLUO_EXIT_U3_ASS_U1_H