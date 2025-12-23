//
// Created by 19571 on 2025/12/23.
//

#ifndef ABOLUO_EXIT_U3_ASS_U1_H
#define ABOLUO_EXIT_U3_ASS_U1_H

#define BRIDGE_STACK_SIZE   1024
#define QUEUE_SIZE          512  // 缓冲区深度，越大约不容易丢包，但耗费RAM

void App_UART_Bridge_Init(void);

#endif //ABOLUO_EXIT_U3_ASS_U1_H