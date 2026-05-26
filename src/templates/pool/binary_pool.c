#include <kl/templates/pool/binary_pool.h>
#include <kl/macros.h>
#include <kl/memory/alloc.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void fill_free_indices(KL_int64_List* indices, int64_t count) {
  for (int64_t i = 0; i < count; i++) kl_int64_list_push(indices, i);
  assert(indices->length == count);
  assert(indices->items != 0);
}

static void ensure_slots(KL_BinaryPool* pool) {
  if (pool->items == 0) {
    pool->items = kl_aligned_alloc(pool->config.element_size, pool->config.element_alignment);
    assert(pool->free_indices.length == 0);
    fill_free_indices(&pool->free_indices, pool->config.count);
  }
  assert(pool->items != 0);
}

void kl_binary_pool_init(KL_BinaryPool* pool, KL_BinaryPoolConfig cfg) {
  if (pool->initialized) return;
  assert(cfg.count > 0);
  assert(cfg.element_size > 0);

  kl_int64_list_init(&pool->free_indices, (KL_ListConfig){
    .capacity = cfg.count,
    .allocator = cfg.allocator,
  });

  kl_bool_hashmap_init(&pool->index_check, (KL_HashmapConfig){
    .capacity = KL_MAX(cfg.count, 5013),
    .collision_capacity = 4,
    .allocator = cfg.allocator,
  });
 
  pool->config = cfg;
  pool->items = 0;
}

void kl_binary_pool_destroy(KL_BinaryPool* pool) {
  assert(pool->initialized);

  kl_int64_list_destroy(&pool->free_indices);

  kl_bool_hashmap_destroy(&pool->index_check);
  
  if (pool->items != 0) {
    free(pool->items);
  }

  memset(pool, 0, sizeof(KL_BinaryPool));
}

void kl_binary_pool_reset(KL_BinaryPool* pool) {
  assert(pool->initialized);
  if (pool->items != 0) {
    memset(pool->items, 0, pool->config.count * pool->config.element_size);
  }
  kl_int64_list_reset(&pool->free_indices);
  fill_free_indices(&pool->free_indices, pool->config.count);
}

uint8_t* kl_binary_pool_acquire_slot(KL_BinaryPool* pool, int64_t* out_idx) {
  assert(pool->initialized);
  ensure_slots(pool);

  *out_idx = -1;

  if (pool->free_indices.length <= 0) return 0;

  kl_int64_list_pop_at(&pool->free_indices, 0, out_idx);
  int64_t idx = *out_idx;
  assert(idx >= 0);
  assert(idx < pool->config.count);

  assert(pool->items != 0);

  kl_bool_hashmap_set(&pool->index_check, KL_BLOB_T(int64_t, idx), true);

  return &pool->items[idx];
}

uint8_t* kl_binary_pool_get_slot(KL_BinaryPool* pool, int64_t idx) {
  assert(pool->initialized);
  if (pool->items == 0) return 0;
  assert(idx >= 0);
  assert(idx < pool->config.count);

  return &pool->items[idx];
}

void kl_binary_pool_release_slot(KL_BinaryPool* pool, int64_t idx) {
  assert(pool->initialized);
  if (pool->free_indices.length <= 0) return;
  if (pool->items == 0) return;

  if (!kl_bool_hashmap_has(&pool->index_check, KL_BLOB_T(int64_t, idx))) return;
  
  kl_bool_hashmap_unset(&pool->index_check, KL_BLOB_T(int64_t, idx));
  
}
