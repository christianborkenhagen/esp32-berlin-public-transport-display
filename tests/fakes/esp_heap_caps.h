#pragma once
#include <stddef.h>
#include <stdint.h>
#define MALLOC_CAP_INTERNAL 1
#define MALLOC_CAP_8BIT 2
#define MALLOC_CAP_DMA 4
#define MALLOC_CAP_SPIRAM 8
void *heap_caps_malloc(size_t size, uint32_t caps);
static inline size_t heap_caps_get_free_size(uint32_t caps) { (void)caps; return 100; }
static inline size_t heap_caps_get_largest_free_block(uint32_t caps) { (void)caps; return 50; }
