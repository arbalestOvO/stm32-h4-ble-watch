//
// Created by 19571 on 2025/12/25.
//

#include "uint8_vector.h"

#include <stdio.h>
#include <stdlib.h>

// 初始化数组
void ba_init(ByteArray *arr, size_t initial_capacity) {
    arr->data = (uint8_t *)malloc(initial_capacity * sizeof(uint8_t));
    if (arr->data == NULL) {
        printf("Memory allocation failed!\n");
        exit(1);
    }
    arr->size = 0;
    arr->capacity = initial_capacity;
}

// 向数组末尾添加元素 (Push Back)
void ba_push(ByteArray *arr, uint8_t value) {
    // 如果容量不足，需要扩容
    if (arr->size >= arr->capacity) {
        // 扩容策略：通常选择翻倍 (x2)，或者初始给个默认值
        size_t new_capacity = (arr->capacity == 0) ? 4 : arr->capacity * 2;

        // 使用临时指针，防止 realloc 失败导致原数据丢失
        uint8_t *new_data = (uint8_t *)realloc(arr->data, new_capacity * sizeof(uint8_t));

        if (new_data == NULL) {
            printf("Memory reallocation failed!\n");
            // 实际项目中这里应该返回错误代码，而不是直接退出
            return;
        }

        arr->data = new_data;
        arr->capacity = new_capacity;
    }

    // 添加数据
    arr->data[arr->size] = value;
    arr->size++;
}

// 获取指定索引的元素
uint8_t ba_get(ByteArray *arr, size_t index) {
    if (index >= arr->size) {
        printf("Index out of bounds!\n");
        return 0; // 或者处理错误
    }
    return arr->data[index];
}

// 设置指定索引的元素
void ba_set(ByteArray *arr, size_t index, uint8_t value) {
    if (index >= arr->size) {
        printf("Index out of bounds!\n");
        return;
    }
    arr->data[index] = value;
}

// 释放数组内存
void ba_free(ByteArray *arr) {
    if (arr->data != NULL) {
        free(arr->data);
        arr->data = NULL;
    }
    arr->size = 0;
    arr->capacity = 0;
}