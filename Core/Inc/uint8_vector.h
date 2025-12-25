//
// Created by 19571 on 2025/12/25.
//

#ifndef ABOLUO_EXIT_UINT8_VECTOR_H
#define ABOLUO_EXIT_UINT8_VECTOR_H
#include <stddef.h>
#include <stdint.h>

typedef struct {
    uint8_t *data;   // 指向实际数据的指针
    size_t size;     // 当前元素个数
    size_t capacity; // 当前分配的内存容量
} ByteArray;

void ba_init(ByteArray *arr, size_t initial_capacity);
void ba_push(ByteArray *arr, uint8_t value);
uint8_t ba_get(ByteArray *arr, size_t index);
void ba_set(ByteArray *arr, size_t index, uint8_t value);
void ba_free(ByteArray *arr);

#endif //ABOLUO_EXIT_UINT8_VECTOR_H