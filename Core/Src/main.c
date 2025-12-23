/* USER CODE BEGIN Header */
/* USER CODE END Header */
/* Includes ------------------------------------------------------------------*/
#include "main.h"
#include "adc.h"
#include "atk_rgblcd.h"
#include "crc.h"
#include "ltdc.h"
#include "memorymap.h"
#include "quadspi.h"
#include "sdmmc.h"
#include "usart.h"
#include "gpio.h"
#include "fmc.h"
#include "printf_impl.h"
#include "sdram.h"
#include "lv_port_disp.h"
#include "src/lv_init.h"
#include "lvgl.h"
#include "stm32h7xx_hal_dma2d.h"
#include "ui.h"

/* Private includes ----------------------------------------------------------*/
/* USER CODE BEGIN Includes */
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

/* USER CODE BEGIN PV */
/* USER CODE END PV */

/* Private function prototypes -----------------------------------------------*/
void SystemClock_Config(void);
static void MPU_Config(void);
/* USER CODE BEGIN PFP */
/* USER CODE END PFP */

/* Private user code ---------------------------------------------------------*/
/* USER CODE BEGIN 0 */
/* USER CODE END 0 */


void lv_example_test(void)
{
  /* 获取当前活动屏幕 */
  lv_obj_t * scr = lv_screen_active();

  /* 1. 测试文字显示 (验证库是否跑起来了) */
  lv_obj_t * label = lv_label_create(scr);
  lv_label_set_text(label, "STM32H7 + LVGL 9.x\nDMA2D Accelerated");
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(label, LV_ALIGN_TOP_MID, 0, 30);
  lv_obj_set_style_bg_color(scr, lv_color_white(), LV_PART_MAIN);
  /* 2. 测试动画 (验证 DMA2D 和 Cache 是否同步)
     如果圆圈转动流畅且没有撕裂/噪点，说明 Cache 清理(Clean)正常
     如果圆圈不动，说明 lv_tick_inc() 没被调用
  */
  lv_obj_t * spinner = lv_spinner_create(scr);
  lv_obj_set_size(spinner, 120, 120);
  lv_obj_center(spinner);

  /* 3. 测试 RGB 颜色顺序 (验证颜色格式)
     很多时候 LCD 硬件是 BGR，而 LVGL 输出 RGB，导致红色变蓝色。
     通过下面三个方块，你可以一眼看出颜色对不对。
  */

  /* 红色方块 */
  lv_obj_t * obj_r = lv_obj_create(scr);
  lv_obj_set_size(obj_r, 60, 60);
  lv_obj_set_style_bg_color(obj_r, lv_palette_main(LV_PALETTE_RED), 0);
  lv_obj_align(obj_r, LV_ALIGN_BOTTOM_LEFT, 20, -20);

  lv_obj_t * label_r = lv_label_create(obj_r);
  lv_label_set_text(label_r, "Red");
  lv_obj_center(label_r);

  /* 绿色方块 */
  lv_obj_t * obj_g = lv_obj_create(scr);
  lv_obj_set_size(obj_g, 60, 60);
  lv_obj_set_style_bg_color(obj_g, lv_palette_main(LV_PALETTE_GREEN), 0);
  lv_obj_align_to(obj_g, obj_r, LV_ALIGN_OUT_RIGHT_MID, 20, 0);

  lv_obj_t * label_g = lv_label_create(obj_g);
  lv_label_set_text(label_g, "Green");
  lv_obj_center(label_g);

  /* 蓝色方块 */
  lv_obj_t * obj_b = lv_obj_create(scr);
  lv_obj_set_size(obj_b, 60, 60);
  lv_obj_set_style_bg_color(obj_b, lv_palette_main(LV_PALETTE_BLUE), 0);
  lv_obj_align_to(obj_b, obj_g, LV_ALIGN_OUT_RIGHT_MID, 20, 0);

  lv_obj_t * label_b = lv_label_create(obj_b);
  lv_label_set_text(label_b, "Blue");
  lv_obj_set_style_text_color(label_b, lv_color_white(), 0); // 蓝底白字
  lv_obj_center(label_b);
}

static void btn_event_cb(lv_event_t * e)
{
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * btn = lv_event_get_target(e);

    if(code == LV_EVENT_CLICKED) {
        static uint8_t cnt = 0;
        cnt++;

        /* 获取按钮里的 Label 对象 */
        lv_obj_t * label = lv_obj_get_child(btn, 0);
        lv_label_set_text_fmt(label, "Clicked: %d", cnt);

        LV_LOG_USER("Button clicked %d", cnt);
    }
}

/* 滑块事件回调：测试拖拽和坐标方向 */
static void slider_event_cb(lv_event_t * e)
{
    lv_obj_t * slider = lv_event_get_target(e);
    lv_obj_t * label = lv_event_get_user_data(e);

    int32_t val = lv_slider_get_value(slider);

    /* 更新滑块上方的文字 */
    lv_label_set_text_fmt(label, "Slider: %d%%", (int)val);
}

/* 创建测试界面主函数 */
void create_touch_test_ui(void)
{
    // 获取当前活动屏幕
    lv_obj_t * scr = lv_screen_active();
    lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_set_style_bg_color(scr, lv_color_white(), LV_PART_MAIN);
    // --- 1. 标题 ---
    lv_obj_t * label_title = lv_label_create(scr);
    lv_label_set_text(label_title, "Touch Test: LVGL v9");
    lv_obj_align(label_title, LV_ALIGN_TOP_MID, 0, 20);
    lv_obj_remove_flag(scr, LV_OBJ_FLAG_CLICKABLE);
    // --- 2. 测试按钮 (点击测试) ---
    lv_obj_t * btn = lv_button_create(scr);
    lv_obj_set_size(btn, 120, 50);
    lv_obj_align(btn, LV_ALIGN_CENTER, 0, -40);
    lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_ALL, NULL);

    lv_obj_t * label_btn = lv_label_create(btn);
    lv_label_set_text(label_btn, "Click Me!");
    lv_obj_center(label_btn);

    // --- 3. 辅助显示滑块数值的 Label ---
    lv_obj_t * label_slider_val = lv_label_create(scr);
    lv_label_set_text(label_slider_val, "Drag the slider");
    lv_obj_align(label_slider_val, LV_ALIGN_CENTER, 0, 30);

    // --- 4. 测试滑块 (拖拽/坐标测试) ---
    lv_obj_t * slider = lv_slider_create(scr);
    lv_obj_set_width(slider, 200);
    lv_obj_align(slider, LV_ALIGN_CENTER, 0, 60);
    // 传入 label 指针作为 user_data，方便在回调里更新文字
    lv_obj_add_event_cb(slider, slider_event_cb, LV_EVENT_VALUE_CHANGED, label_slider_val);

    // --- 5. 测试开关 (精准度测试) ---
    lv_obj_t * sw = lv_switch_create(scr);
    lv_obj_align(sw, LV_ALIGN_BOTTOM_MID, 0, -20);
}


DMA2D_HandleTypeDef hdma2d;
extern lv_display_t * disp_refr;
void MX_DMA2D_Init(void)
{
  /* 1. 开启时钟 */
  __HAL_RCC_DMA2D_CLK_ENABLE();

  /* 2. 配置句柄 */
  hdma2d.Instance = DMA2D;
  hdma2d.Init.Mode = DMA2D_M2M;             // 内存到内存模式
  hdma2d.Init.ColorMode = DMA2D_OUTPUT_RGB565; // !!! 必须与 LVGL LV_COLOR_DEPTH 匹配 !!!
  hdma2d.Init.OutputOffset = 0;             // 输出偏移，初始为0

  // 下面这些通常用于特定图层混合，初始化时设为默认即可
  hdma2d.LayerCfg[1].InputOffset = 0;
  hdma2d.LayerCfg[1].InputColorMode = DMA2D_INPUT_RGB565;
  hdma2d.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;
  hdma2d.LayerCfg[1].InputAlpha = 0;

  /* 3. 调用 HAL 初始化 */
  if (HAL_DMA2D_Init(&hdma2d) != HAL_OK)
  {
    Error_Handler();
  }

  /* 4. 开启中断 (如果你使用了 LVGL 的中断等待机制) */
  HAL_NVIC_SetPriority(DMA2D_IRQn, 0, 0); // 优先级根据需要调整
  HAL_NVIC_EnableIRQ(DMA2D_IRQn);
}

extern DMA2D_HandleTypeDef hdma2d;

void DMA2D_IRQHandler(void)
{
  HAL_DMA2D_IRQHandler(&hdma2d);
}

void App_Fix_Screen_Opacity(void)
{
  /* 1. 获取当前活动屏幕 */
  lv_obj_t * scr = lv_screen_active(); // v9 新写法，v8用 lv_scr_act()

  /* 2. 移除该屏幕的所有样式 (清除默认主题可能带来的透明圆角等干扰) */
  lv_obj_remove_style_all(scr);

  /* 3. 重新赋予最基础的样式：绝对不透明，黑色背景 */
  lv_obj_set_style_bg_opa(scr, LV_OPA_COVER, LV_PART_MAIN);
  lv_obj_set_style_bg_color(scr, lv_color_black(), LV_PART_MAIN);

  /* 4. 确保大小占满全屏 */
  lv_obj_set_size(scr, 1024, 600);
}

void Force_Black_Bg_cb(lv_event_t * e)
{
  lv_layer_t * layer = lv_event_get_layer(e);
  lv_area_t area;
  lv_area_copy(&area, &layer->_clip_area);

  /* 只要发生重绘，就先用黑色填满这个区域 */
  /* 相当于局部 memset，但这是 LVGL 内部机制，很快 */
  lv_draw_rect_dsc_t rect_dsc;
  lv_draw_rect_dsc_init(&rect_dsc);
  rect_dsc.bg_color = lv_color_black();
  rect_dsc.bg_opa = LV_OPA_COVER;

  lv_draw_rect(layer, &rect_dsc, &area);
}

/**
  * @brief  The application entry point.
  * @retval int
  */
int main(void)
{

  /* USER CODE BEGIN 1 */
  /* USER CODE END 1 */

  /* MPU Configuration--------------------------------------------------------*/
  MPU_Config();

  /* MCU Configuration--------------------------------------------------------*/

  /* Reset of all peripherals, Initializes the Flash interface and the Systick. */
  HAL_Init();

  /* USER CODE BEGIN Init */
  /* USER CODE END Init */

  /* Configure the system clock */
  SystemClock_Config();

  /* USER CODE BEGIN SysInit */
  /* USER CODE END SysInit */
  SCB_EnableICache(); // 开启指令缓存 (让代码跑得更快)
  SCB_EnableDCache(); // 开启数据缓存 (让数据读写更快)
  /* Initialize all configured peripherals */
  MX_GPIO_Init();
  MX_ADC1_Init();
  MX_FMC_Init();
  MX_QUADSPI_Init();
  MX_SDMMC1_SD_Init();
  MX_USART1_UART_Init();
  MX_USART3_UART_Init();
  MX_LTDC_Init();
  MX_CRC_Init();
  MX_DMA2D_Init();


  sdram_init();
  atk_rgblcd_init();
  atk_rgblcd_display_on();
  atk_rgblcd_clear(ATK_RGBLCD_WHITE);
  lv_init();
  lv_port_disp_init();
  lv_port_touch_init();
  // create_touch_test_ui();
  // lv_example_test();
  /* USER CODE BEGIN 2 */
  /* USER CODE END 2 */
  ui_init();
  // lv_obj_t * scr = lv_screen_active();
  // lv_obj_add_event_cb(scr, Force_Black_Bg_cb, LV_EVENT_DRAW_MAIN_BEGIN, NULL);
  // /* 必须让屏幕对象无效化一次，触发重绘 */
  // lv_obj_invalidate(scr);
  /* Infinite loop */
  /* USER CODE BEGIN WHILE */
  while (1)
  {
    /* USER CODE END WHILE */
    lv_timer_handler(); // 处理UI绘制任务
    HAL_Delay(5);       // 必须有短暂延时，给系统喘息
    /* USER CODE BEGIN 3 */
  }
  /* USER CODE END 3 */
}

/**
  * @brief System Clock Configuration
  * @retval None
  */
void SystemClock_Config(void)
{
  RCC_OscInitTypeDef RCC_OscInitStruct = {0};
  RCC_ClkInitTypeDef RCC_ClkInitStruct = {0};

  /** Supply configuration update enable
  */
  HAL_PWREx_ConfigSupply(PWR_LDO_SUPPLY);

  /** Configure the main internal regulator output voltage
  */
  __HAL_PWR_VOLTAGESCALING_CONFIG(PWR_REGULATOR_VOLTAGE_SCALE1);

  while(!__HAL_PWR_GET_FLAG(PWR_FLAG_VOSRDY)) {}

  /** Initializes the RCC Oscillators according to the specified parameters
  * in the RCC_OscInitTypeDef structure.
  */
  RCC_OscInitStruct.OscillatorType = RCC_OSCILLATORTYPE_HSE;
  RCC_OscInitStruct.HSEState = RCC_HSE_ON;
  RCC_OscInitStruct.PLL.PLLState = RCC_PLL_ON;
  RCC_OscInitStruct.PLL.PLLSource = RCC_PLLSOURCE_HSE;
  RCC_OscInitStruct.PLL.PLLM = 5;
  RCC_OscInitStruct.PLL.PLLN = 160;
  RCC_OscInitStruct.PLL.PLLP = 2;
  RCC_OscInitStruct.PLL.PLLQ = 4;
  RCC_OscInitStruct.PLL.PLLR = 4;
  RCC_OscInitStruct.PLL.PLLRGE = RCC_PLL1VCIRANGE_2;
  RCC_OscInitStruct.PLL.PLLVCOSEL = RCC_PLL1VCOWIDE;
  RCC_OscInitStruct.PLL.PLLFRACN = 0;
  if (HAL_RCC_OscConfig(&RCC_OscInitStruct) != HAL_OK)
  {
    Error_Handler();
  }

  /** Initializes the CPU, AHB and APB buses clocks
  */
  RCC_ClkInitStruct.ClockType = RCC_CLOCKTYPE_HCLK|RCC_CLOCKTYPE_SYSCLK
                              |RCC_CLOCKTYPE_PCLK1|RCC_CLOCKTYPE_PCLK2
                              |RCC_CLOCKTYPE_D3PCLK1|RCC_CLOCKTYPE_D1PCLK1;
  RCC_ClkInitStruct.SYSCLKSource = RCC_SYSCLKSOURCE_PLLCLK;
  RCC_ClkInitStruct.SYSCLKDivider = RCC_SYSCLK_DIV1;
  RCC_ClkInitStruct.AHBCLKDivider = RCC_HCLK_DIV2;
  RCC_ClkInitStruct.APB3CLKDivider = RCC_APB3_DIV4;
  RCC_ClkInitStruct.APB1CLKDivider = RCC_APB1_DIV4;
  RCC_ClkInitStruct.APB2CLKDivider = RCC_APB2_DIV4;
  RCC_ClkInitStruct.APB4CLKDivider = RCC_APB4_DIV4;

  if (HAL_RCC_ClockConfig(&RCC_ClkInitStruct, FLASH_LATENCY_2) != HAL_OK)
  {
    Error_Handler();
  }
  HAL_EnableCompensationCell();
}

/* USER CODE BEGIN 4 */
/* USER CODE END 4 */

 /* MPU Configuration */

void MPU_Config(void)
{
  MPU_Region_InitTypeDef MPU_InitStruct = {0};

  /* Disables the MPU */
  HAL_MPU_Disable();

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Enable = MPU_REGION_ENABLE;
  MPU_InitStruct.Number = MPU_REGION_NUMBER0;
  MPU_InitStruct.BaseAddress = 0x0;
  MPU_InitStruct.Size = MPU_REGION_SIZE_4GB;
  MPU_InitStruct.SubRegionDisable = 0x87;
  MPU_InitStruct.TypeExtField = MPU_TEX_LEVEL0;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Number = MPU_REGION_NUMBER1;
  MPU_InitStruct.BaseAddress = 0x20000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_128KB;
  MPU_InitStruct.SubRegionDisable = 0x0;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_ENABLE;
  MPU_InitStruct.IsShareable = MPU_ACCESS_NOT_SHAREABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Number = MPU_REGION_NUMBER2;
  MPU_InitStruct.BaseAddress = 0x24000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_512KB;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Number = MPU_REGION_NUMBER3;
  MPU_InitStruct.BaseAddress = 0x30000000;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Number = MPU_REGION_NUMBER4;
  MPU_InitStruct.BaseAddress = 0x38000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_64KB;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Number = MPU_REGION_NUMBER5;
  MPU_InitStruct.BaseAddress = 0x60000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_64MB;
  MPU_InitStruct.AccessPermission = MPU_REGION_NO_ACCESS;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Number = MPU_REGION_NUMBER6;
  MPU_InitStruct.BaseAddress = 0xC0000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_32MB;
  MPU_InitStruct.AccessPermission = MPU_REGION_FULL_ACCESS;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);

  /** Initializes and configures the Region and the memory to be protected
  */
  MPU_InitStruct.Number = MPU_REGION_NUMBER7;
  MPU_InitStruct.BaseAddress = 0x80000000;
  MPU_InitStruct.Size = MPU_REGION_SIZE_256MB;
  MPU_InitStruct.DisableExec = MPU_INSTRUCTION_ACCESS_DISABLE;
  MPU_InitStruct.IsCacheable = MPU_ACCESS_NOT_CACHEABLE;
  MPU_InitStruct.IsBufferable = MPU_ACCESS_NOT_BUFFERABLE;

  HAL_MPU_ConfigRegion(&MPU_InitStruct);
  /* Enables the MPU */
  HAL_MPU_Enable(MPU_PRIVILEGED_DEFAULT);

}

/**
  * @brief  This function is executed in case of error occurrence.
  * @retval None
  */
void Error_Handler(void)
{
  /* USER CODE BEGIN Error_Handler_Debug */
  /* USER CODE END Error_Handler_Debug */
}

#ifdef  USE_FULL_ASSERT
/**
  * @brief  Reports the name of the source file and the source line number
  *         where the assert_param error has occurred.
  * @param  file: pointer to the source file name
  * @param  line: assert_param error line source number
  * @retval None
  */
void assert_failed(uint8_t *file, uint32_t line)
{
  /* USER CODE BEGIN 6 */
  /* USER CODE END 6 */
}
#endif /* USE_FULL_ASSERT */
