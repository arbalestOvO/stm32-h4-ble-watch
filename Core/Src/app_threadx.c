/* USER CODE BEGIN Header */
/**
  ******************************************************************************
  * @file    app_threadx.c
  * @author  MCD Application Team
  * @brief   ThreadX applicative file
  ******************************************************************************
  * @attention
  *
  * Copyright (c) 2020-2021 STMicroelectronics.
  * All rights reserved.
  *
  * This software is licensed under terms that can be found in the LICENSE file
  * in the root directory of this software component.
  * If no LICENSE file comes with this software, it is provided AS-IS.
  *
  ******************************************************************************
  */
/* USER CODE END Header */

/* Includes ------------------------------------------------------------------*/
#include "app_threadx.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
#include <stdio.h>
#include "stm32h7xx_hal.h"
#include "ui_message_handler.h"
#include "build/Release/_deps/lvgl-src/src/misc/lv_timer.h"
#include "ui_protocol_type.h"
/* USER CODE END Includes */

/* Private typedef -----------------------------------------------------------*/
/* USER CODE BEGIN PTD */

/* USER CODE END PTD */

/* Private define ------------------------------------------------------------*/
/* USER CODE BEGIN PD */

/* USER CODE END PD */

/* Private macro -------------------------------------------------------------*/
/* USER CODE BEGIN PM */

/* USER CODE END PM */

/* Private variables ---------------------------------------------------------*/
TX_THREAD tx_app_thread;
TX_QUEUE g_ui_queue;
uint8_t g_ui_queue_buffer[UI_QUEUE_SIZE * sizeof(ui_message_t)];
/* USER CODE BEGIN PV */
TX_MUTEX lvgl_mutex;

void gui_init_threadx_setup() {
  tx_mutex_create(&lvgl_mutex, "LVGL Mutex", TX_NO_INHERIT);
}
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
/* USER CODE BEGIN PFP */
void await_and_handle_queue(void);
TX_THREAD * g_gui_thread_ptr;
/* USER CODE END PFP */
/**
  * @brief  Application ThreadX Initialization.
  * @param memory_ptr: memory pointer
  * @retval int
  */
UINT App_ThreadX_Init(VOID *memory_ptr)
{
  UINT ret = TX_SUCCESS;
  TX_BYTE_POOL *byte_pool = (TX_BYTE_POOL*)memory_ptr;
  /* USER CODE BEGIN App_ThreadX_MEM_POOL */

  /* USER CODE END App_ThreadX_MEM_POOL */
  CHAR *pointer;

  /* Allocate the stack for tx app thread  */
  if (tx_byte_allocate(byte_pool, (VOID**) &pointer,
                       TX_APP_STACK_SIZE, TX_NO_WAIT) != TX_SUCCESS)
  {
    return TX_POOL_ERROR;
  }
  /* Create tx app thread.  */
  if (tx_thread_create(&tx_app_thread, "tx app thread", tx_app_thread_entry, 0, pointer,
                       TX_APP_STACK_SIZE, TX_APP_THREAD_PRIO, TX_APP_THREAD_PREEMPTION_THRESHOLD,
                       TX_APP_THREAD_TIME_SLICE, TX_APP_THREAD_AUTO_START) != TX_SUCCESS)
  {
    return TX_THREAD_ERROR;
  }

  /* USER CODE BEGIN App_ThreadX_Init */
  gui_init_threadx_setup();
  UINT msg_size_in_words = sizeof(ui_message_t) / 4;
  if (sizeof(ui_message_t) % 4 != 0) msg_size_in_words++;

  tx_queue_create(&g_ui_queue, "Global UI Queue", msg_size_in_words,
                  g_ui_queue_buffer, sizeof(g_ui_queue_buffer));
  /* USER CODE END App_ThreadX_Init */

  return ret;
}
/**
  * @brief  Function implementing the tx_app_thread_entry thread.
  * @param  thread_input: Hardcoded to 0.
  * @retval None
  */
void tx_app_thread_entry(ULONG thread_input)
{
  /* USER CODE BEGIN tx_app_thread_entry */
  g_gui_thread_ptr = tx_thread_identify();
  while (1) {
    tx_mutex_get(&lvgl_mutex, TX_WAIT_FOREVER);
    lv_timer_handler(); // 处理UI绘制任务
    tx_mutex_put(&lvgl_mutex);
    await_and_handle_queue();
    tx_thread_sleep(3);       // 必须有短暂延时，给系统喘息
  }
  /* USER CODE END tx_app_thread_entry */
}

  /**
  * @brief  Function that implements the kernel's initialization.
  * @param  None
  * @retval None
  */
void MX_ThreadX_Init(void)
{
  /* USER CODE BEGIN  Before_Kernel_Start */

  /* USER CODE END  Before_Kernel_Start */

  tx_kernel_enter();

  /* USER CODE BEGIN  Kernel_Start_Error */

  /* USER CODE END  Kernel_Start_Error */
}

/* USER CODE BEGIN 1 */
void await_and_handle_queue(void) {
  ui_message_t recv_msg;
  while (tx_queue_receive(&g_ui_queue, &recv_msg, TX_NO_WAIT) == TX_SUCCESS) {

    // --- 分发器逻辑 ---
    switch (recv_msg.type) {
      case UI_EVENT_BLE_FOUND:
        UI_AddBleItem(recv_msg.payload.ble.name, recv_msg.payload.ble.mac);
        break;
      default:
        // 未知消息类型
        break;
    }
  }
}
/* USER CODE END 1 */
