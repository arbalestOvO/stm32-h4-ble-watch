#ifndef __RANDOM_UTILS_H__
#define __RANDOM_UTILS_H__

#include "main.h" // 包含 main.h 以获取 HAL 库定义

// 依赖：需要确保 main.c 中定义了 RNG_HandleTypeDef hrng;

/**
 * @brief 获取一个 32 位的随机整数
 * @return 32位随机数
 */
uint32_t Random_GetUint32(void);

/**
 * @brief 获取指定范围内的随机数 [min, max]
 * @param min 最小值
 * @param max 最大值
 * @return 范围内的一个随机数
 */
int32_t Random_GetRange(int32_t min, int32_t max);

/**
 * @brief 生成一个随机数数组
 * @param pBuffer 接收数组的指针
 * @param length  数组长度
 */
void Random_GetArray(uint32_t *pBuffer, uint16_t length);

void Random_GetByteArray(uint8_t *pBuffer, uint16_t length);

#endif /* __RANDOM_UTILS_H__ */