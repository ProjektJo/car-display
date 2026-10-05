#pragma once
#include <cstdlib>
#define MALLOC_CAP_DMA 1
#define MALLOC_CAP_INTERNAL 2
#define MALLOC_CAP_SPIRAM 4
#define MALLOC_CAP_8BIT 8
inline void* heap_caps_malloc(size_t n, unsigned) { return malloc(n); }
inline size_t heap_caps_get_free_size(unsigned) { return 0; }
