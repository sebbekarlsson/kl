#ifndef KL_ALLOC_H
#define KL_ALLOC_H
#include <stdlib.h>

void *kl_aligned_alloc(size_t size, size_t alignment);

void *kl_aligned_realloc(void* ptr, size_t new_size, size_t alignment);

#endif
