#ifndef KL_LIST_SHARED
#define KL_LIST_SHARED
#include <stdint.h>
#include <kl/memory/allocator.h>

typedef struct {
  int64_t capacity;
  KL_Allocator* allocator;
} KL_ListConfig;
#endif
