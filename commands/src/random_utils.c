#include "random_utils.h"
#include <stdlib.h> // 用于 abs 函数

// 引用 CubeMX 生成的全局 RNG 句柄
extern RNG_HandleTypeDef hrng;

/**
 * @brief 获取一个 32 位硬件随机数
 * @note 如果硬件出错，这里会返回 0，实际生产环境建议增加超时重试机制
 */
uint32_t Random_GetUint32(void)
{
    uint32_t randomValue = 0;
    
    // 使用 HAL 库生成随机数
    // HAL_RNG_GenerateRandomNumber 会检查 DRDY 标志位
    if (HAL_RNG_GenerateRandomNumber(&hrng, &randomValue) != HAL_OK)
    {
        // 错误处理：可以尝试重置 RNG 或返回错误码
        // 这里简单处理：如果失败返回 0 (或者你可以由系统 tick 兜底)
        return 0; 
    }
    
    return randomValue;
}

/**
 * @brief 获取指定范围内的随机数 [min, max]
 */
int32_t Random_GetRange(int32_t min, int32_t max)
{
    if (min >= max) return min;

    uint32_t raw = Random_GetUint32();
    
    // 使用取模法限制范围
    // 注意：简单的取模会引入轻微的偏差，但对非加密场景通常足够
    return (int32_t)(raw % (max - min + 1)) + min;
}

/**
 * @brief 生成随机数组
 */
void Random_GetArray(uint32_t *pBuffer, uint16_t length)
{
    if (pBuffer == NULL || length == 0) return;

    for (uint16_t i = 0; i < length; i++)
    {
        pBuffer[i] = Random_GetUint32();
    }
}

void Random_GetByteArray(uint8_t *pBuffer, uint16_t length) {
    if (pBuffer == NULL || length == 0) return;

    uint32_t i = 0;
    uint32_t rand32 = 0;

    while (i < length)
    {
        // 每填充 4 个字节（或者刚开始时），获取一个新的 32 位随机数
        // (i & 0x03) 等同于 (i % 4)
        if ((i & 0x03) == 0)
        {
            rand32 = Random_GetUint32();
        }

        // 提取当前需要的字节
        // 根据 i 的低2位 (0,1,2,3) 决定右移多少位 (0,8,16,24)
        pBuffer[i] = (uint8_t)((rand32 >> ((i & 0x03) * 8)) & 0xFF);

        i++;
    }
}