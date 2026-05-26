#ifndef KL_BINARY_POOL_H
#define KL_BINARY_POOL_H

#include <kl/templates/list/int64_list.h>
#include <kl/threads/mutex.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
  int64_t count;
  int64_t element_size;
  int64_t element_alignment;

  KL_Allocator* allocator; // optional
} KL_BinaryPoolConfig;

typedef struct {
  bool initialized;
  KL_BinaryPoolConfig config;
  
  uint8_t* items;
  KL_int64_List free_indices;
  KL_int64_List taken_indices;

  // For each slot: -1 if free, otherwise the position of that slot's index
  // inside `taken_indices`. Doubles as the "is taken?" check and as a
  // direct pointer for O(1) swap-and-pop on release.
  int64_t* taken_pos;

  KL_Mutex lock;
} KL_BinaryPool;

bool kl_binary_pool_init(KL_BinaryPool* pool, KL_BinaryPoolConfig cfg);

void kl_binary_pool_destroy(KL_BinaryPool* pool);

void kl_binary_pool_reset(KL_BinaryPool* pool);

uint8_t* kl_binary_pool_acquire_slot(KL_BinaryPool* pool, int64_t* out_idx);

uint8_t* kl_binary_pool_get_slot(KL_BinaryPool* pool, int64_t idx);

void kl_binary_pool_release_slot(KL_BinaryPool* pool, int64_t idx);


#endif
