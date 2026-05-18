#ifndef KL_SPARSE_SET_CONFIG__SHARED
#define KL_SPARSE_SET_CONFIG__SHARED
#include <kl/memory/allocator.h>
#include <stdint.h>
typedef struct {
  int64_t sparse_capacity;
  int64_t dense_capacity;
  KL_Allocator* allocator;
} KL_SparseSetConfig;
#endif
