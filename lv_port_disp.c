/* 包含必要的头文件 */
#include <stdio.h>
#include <string.h>

#include "lvgl.h"
#include "stm32h7xx.h" /* 包含寄存器定义 */
#include "atk_rgblcd_ltdc.h"
#include "stm32h7xx_hal_dma2d.h"
#include "atk_rgblcd_touch.h"
#include "tx_api.h"

#define LCD_FRAMEBUFFER_START_ADDR  0xC0000000
#define LCD_WIDTH_PHY               1024

extern LTDC_HandleTypeDef hltdc;
lv_display_t * disp_refr = NULL;
// void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t * px_map)
// {
//     int32_t width = lv_area_get_width(area);
//     int32_t height = lv_area_get_height(area);
//     int32_t data_size = width * height * 2;
//     SCB_CleanDCache_by_Addr((uint32_t*)px_map, data_size);
//     HAL_LTDC_SetAddress(&hltdc, (uint32_t)px_map, 1);
//     disp_refr = disp;
//     __HAL_LTDC_ENABLE_IT(&hltdc, LTDC_IT_RR);
//
//     HAL_LTDC_Reload(&hltdc, LTDC_RELOAD_VERTICAL_BLANKING);
//     // lv_display_flush_ready(disp_refr);
// }


/* 假设屏幕是 RGB565 (16bit)，如果是 ARGB8888 请改为 4 */
#define LV_COLOR_SIZE       2
#define LCD_WIDTH           1024
#define LCD_FRAME_BUFFER    0xC0000000 // 你的LCD层显存首地址

extern DMA2D_HandleTypeDef hdma2d;
static lv_display_t * g_disp_flushing = NULL; // 用于在中断里通知 LVGL

// DMA2D 传输完成中断回调
void DMA2D_TransferCompleteCallback(DMA2D_HandleTypeDef *hdma2d)
{
    /* 只有当正在进行 LVGL 刷新时才处理 */
    if (g_disp_flushing != NULL)
    {
        /* 通知 LVGL 刷新结束 (这是必须的！) */
        lv_display_flush_ready(g_disp_flushing);
        g_disp_flushing = NULL;
    }
}


void my_disp_flush(lv_display_t *disp, const lv_area_t *area, uint8_t * px_map)
{
    int32_t width = lv_area_get_width(area);
    int32_t height = lv_area_get_height(area);
    SCB_CleanDCache_by_Addr((uint32_t*)px_map, width * height * 2);
    g_disp_flushing = disp; // 供中断使用

    /* 计算目标地址 (RGB565) */
    uint32_t dest_addr = LCD_FRAME_BUFFER + (area->y1 * LCD_WIDTH + area->x1) * 2;
    hdma2d.Init.Mode = DMA2D_M2M;
    hdma2d.Init.ColorMode = DMA2D_OUTPUT_RGB565;
    hdma2d.Init.OutputOffset = LCD_WIDTH - width;
    // printf("dest addr: %X %X width: %d\n", dest_addr, (uint32_t)px_map, hdma2d.Init.OutputOffset);
    /* 必须重新 Init 以应用 Offset (虽然慢一点点，但比寄存器混写安全) */
    /* 为了追求极限性能，这里可以用寄存器改 Offset，然后手动改 hdma2d.State，但不推荐 */
    if(HAL_DMA2D_Init(&hdma2d) != HAL_OK)
    {
        Error_Handler();
    }

    hdma2d.LayerCfg[1].InputOffset = 0;
    hdma2d.LayerCfg[1].InputColorMode = DMA2D_INPUT_RGB565;
    hdma2d.LayerCfg[1].AlphaMode = DMA2D_NO_MODIF_ALPHA;

    /* 直接配置层参数，比完全 HAL_DMA2D_ConfigLayer 快 */
    hdma2d.Instance->FGPFCCR = DMA2D_INPUT_RGB565;
    hdma2d.Instance->FGOR = 0;

    /* 使用 Start_IT 启动，这样 HAL 库会将 State 设为 BUSY，中断才会正常工作 */
    if (HAL_DMA2D_Start_IT(&hdma2d, (uint32_t)px_map, dest_addr, width, height) != HAL_OK)
    {
        /* 出错兜底 */
        lv_display_flush_ready(disp);
    }
}

void LTDC_IRQHandler(void)
{
    HAL_LTDC_IRQHandler(&hltdc);
}

// 这是一个 HAL 库的回调函数，当 Reload 完成（即 VSYNC 到来）时被调用
void HAL_LTDC_ReloadEventCallback(LTDC_HandleTypeDef *hltdc)
{
    if(disp_refr != NULL) {
        // 通知 LVGL：刚才那帧已经上屏了，你可以复用这块 buffer 了
        lv_display_flush_ready(disp_refr);
        disp_refr = NULL;
    }

    // 如果不需要每帧都进中断，可以在这里关闭中断，下次 flush 再开启
    __HAL_LTDC_DISABLE_IT(hltdc, LTDC_IT_RR);
}

#define MY_DISP_HOR_RES 1024
#define MY_DISP_VER_RES 600

#define SDRAM_BANK_ADDR    0xC0000000

/* 定义显存和缓冲区的偏移 */
/* 显存(全屏)占用: 800*480*2 = 768,000 Bytes (约 750KB) */
#define LCD_FRAME_BUF_ADDR (SDRAM_BANK_ADDR)

/* LVGL 绘图缓冲(1/10屏)占用: 768,000 / 10 = 76,800 Bytes */
/* 把它放在显存后面，防止覆盖 */
// #define LVGL_DRAW_BUF1_ADDR (SDRAM_BANK_ADDR + MY_DISP_HOR_RES*MY_DISP_VER_RES*2)
// #define LVGL_DRAW_BUF2_ADDR (LVGL_DRAW_BUF1_ADDR + MY_DISP_HOR_RES*MY_DISP_VER_RES*2/10)


#define LCD_FB_SIZE (MY_DISP_HOR_RES * MY_DISP_VER_RES * 2) // RGB565

/* 定义两个显存地址 (SDRAM) */
#define LCD_FB_ADDR_1  0xC0000000
#define LCD_FB_ADDR_2  (0xC0000000 + LCD_FB_SIZE)

#define DRAW_BUF_SIZE_BYTES (MY_DISP_HOR_RES * MY_DISP_VER_RES * 2 / 10)
#define LVGL_DRAW_BUF1_ADDR (0xC0200000)
#define LVGL_DRAW_BUF2_ADDR (0xC0200000 + DRAW_BUF_SIZE_BYTES + 64) // +64 为了安全间隔

/* 指针转换 */
static uint16_t *buf_1 = (uint16_t *)LVGL_DRAW_BUF1_ADDR;
static uint16_t *buf_2 = (uint16_t *)LVGL_DRAW_BUF2_ADDR;

void lv_port_disp_init(void)
{
   void *buf1 = (void *)LCD_FB_ADDR_1;
   void *buf2 = (void *)LCD_FB_ADDR_2;
   HAL_DMA2D_RegisterCallback(&hdma2d, HAL_DMA2D_TRANSFERCOMPLETE_CB_ID, DMA2D_TransferCompleteCallback);

   lv_display_t * disp = lv_display_create(MY_DISP_HOR_RES, MY_DISP_VER_RES);

   lv_display_set_flush_cb(disp, my_disp_flush);

   lv_display_set_color_format(disp, LV_COLOR_FORMAT_RGB565);

   /* !!! 关键修改 !!! */
   /* 使用 DIRECT 模式：LVGL 直接在显存上画图，不使用局部小 buffer */
   // lv_display_set_buffers(disp, buf1, buf2, LCD_FB_SIZE, LV_DISPLAY_RENDER_MODE_DIRECT);

    lv_display_set_buffers(disp, buf_1, buf_2, MY_DISP_HOR_RES * MY_DISP_VER_RES * 2 / 10, LV_DISPLAY_RENDER_MODE_PARTIAL);
}

void my_touchpad_read(lv_indev_t * indev, lv_indev_data_t * data)
{
    static atk_rgblcd_touch_point_t tp_data;
    static int16_t last_x = 0;
    static int16_t last_y = 0;
    UINT old_interrupt_posture;

    old_interrupt_posture = tx_interrupt_control(TX_INT_DISABLE);
    uint8_t hardware_detected = atk_rgblcd_touch_scan(&tp_data, 1);
    tx_interrupt_control(old_interrupt_posture);
    if (hardware_detected) {
        printf("x: %d, y: %d\n", tp_data.x, tp_data.y);
    }
    if(hardware_detected) {
        bool is_valid = true;

        if(tp_data.x == 0 && tp_data.y == 0) is_valid = false;

        if(tp_data.x >= LCD_WIDTH || tp_data.y >= MY_DISP_VER_RES) is_valid = false;

        if(is_valid) {
            last_x = tp_data.x;
            last_y = tp_data.y;
        }

        data->state = LV_INDEV_STATE_PRESSED;
        data->point.x = last_x;
        data->point.y = last_y;

    } else {
        data->state = LV_INDEV_STATE_RELEASED;

        data->point.x = last_x;
        data->point.y = last_y;
    }
}

void lv_port_touch_init(void)
{
    atk_rgblcd_touch_init(ATK_RGBLCD_TOUCH_TYPE_GTXX);
    lv_indev_t *lv_indev = lv_indev_create();
    lv_indev_set_type(lv_indev, LV_INDEV_TYPE_POINTER);
    // 3. 绑定刚才写的回调函数
    lv_indev_set_read_cb(lv_indev, my_touchpad_read);
}