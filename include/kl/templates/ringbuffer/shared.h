#ifndef KL_RINGBUFFER_CONFIG__SHARED
#define KL_RINGBUFFER_CONFIG__SHARED
#include <kl/memory/allocator.h>
#include <stdint.h>
typedef struct {
  int64_t length;
  KL_Allocator* allocator; // optional
} KL_RingBufferConfig;
#endif
