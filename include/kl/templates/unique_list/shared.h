#ifndef KL_UNIQUE_LIST_SHARED
#define KL_UNIQUE_LIST_SHARED
#include <stdint.h>
#include <kl/memory/allocator.h>

typedef struct {
  int64_t capacity;
  int64_t collision_capacity;
  KL_Allocator* allocator;
} KL_UniqueListConfig;
#endif
