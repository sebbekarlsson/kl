#ifndef KL_QUEUE_CONFIG__SHARED
#define KL_QUEUE_CONFIG__SHARED
#include <stdint.h>
#include <kl/memory/allocator.h>

typedef struct {
  int64_t capacity;
  int64_t max_length;
  KL_Allocator* allocator;
} KL_QueueConfig;
#endif
