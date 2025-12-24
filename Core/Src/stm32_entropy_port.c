/*
 * Copyright (C) 2024. All rights reserved.
 * * This file provides the platform-specific entropy source for Mbed TLS/PSA Crypto
 * on STM32H7 devices using the Hardware Random Number Generator (RNG).
 *
 * It implements mbedtls_platform_get_entropy() which is required when
 * MBEDTLS_PSA_DRIVER_GET_ENTROPY is defined in crypto_config.h.
 */

#include "main.h"            // 包含 CubeMX 生成的主头文件，通常定义了 HAL 句柄
#include "stm32h7xx_hal.h"   // 引入 STM32H7 HAL 库
#include <string.h>          // 用于 memcpy

#include "psa/crypto_driver_random.h"


/* * 引入 CubeMX 生成的全局 RNG 句柄
 * 如果你的句柄名称不同（例如 hrng1），请在此处修改。
 */
extern RNG_HandleTypeDef hrng;

/**
 * @brief           TF-PSA-Crypto 平台熵获取回调函数 (新版接口)
 *
 * @note            当配置中启用了 MBEDTLS_PSA_DRIVER_GET_ENTROPY 时调用。
 * 适配最新的 TF-PSA-Crypto 驱动签名。
 *
 * @param flags         熵获取标志 (psa_driver_get_entropy_flags_t)
 * @param estimate_bits 用于回传实际收集到的熵位数 (输出参数)
 * @param output        用于存储熵数据的输出缓冲区
 * @param output_size   输出缓冲区的大小
 *
 * @return          PSA_SUCCESS (0) 表示成功，其他值表示错误
 */
int mbedtls_platform_get_entropy(psa_driver_get_entropy_flags_t flags,
                                 size_t *estimate_bits,
                                 unsigned char *output,
                                 size_t output_size)
{
    uint32_t random_32bit = 0;
    size_t bytes_generated = 0;

    (void)flags; // 本实现忽略 flags，总是尽力获取

    /* 初始化估算熵值为 0 */
    if (estimate_bits != NULL) {
        *estimate_bits = 0;
    }

    /* 简单的状态检查，确保 RNG 外设已初始化 */
    if (hrng.State == HAL_RNG_STATE_RESET)
    {
        return PSA_ERROR_HARDWARE_FAILURE;
    }

    /* 循环生成随机数直到填满请求的长度 */
    while (bytes_generated < output_size)
    {
        /* * 调用 STM32 HAL 库生成一个 32 位随机数
         * HAL_RNG_GenerateRandomNumber 会自动处理 RNG_SR 状态寄存器的检查
         */
        if (HAL_RNG_GenerateRandomNumber(&hrng, &random_32bit) != HAL_OK)
        {
            return PSA_ERROR_HARDWARE_FAILURE;
        }

        /* 计算剩余需要的字节数 */
        size_t remaining = output_size - bytes_generated;

        /* 每次最多拷贝 4 字节（32位） */
        size_t chunk_size = (remaining < 4) ? remaining : 4;

        /* 将生成的随机数拷贝到输出缓冲区 */
        memcpy(output + bytes_generated, &random_32bit, chunk_size);

        bytes_generated += chunk_size;
    }

    /* * 更新实际估算的熵位数。
     * 假设硬件 RNG 产生的都是全熵数据 (1 byte = 8 bits entropy)。
     */
    if (estimate_bits != NULL) {
        *estimate_bits = bytes_generated * 8;
    }

    return PSA_SUCCESS;
}