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

static int64_t pool_stride(const KL_BinaryPool* pool) {
  int64_t sz = pool->config.element_size;
  int64_t al = pool->config.element_alignment;
  if (al <= 1) return sz;
  return (sz + al - 1) & ~(al - 1);
}

static void ensure_slots(KL_BinaryPool* pool) {
  if (pool->items == 0) {
    int64_t stride = pool_stride(pool);
    pool->items = kl_aligned_alloc(pool->config.count * stride, pool->config.element_alignment);
    assert(pool->free_indices.length == 0);
    fill_free_indices(&pool->free_indices, pool->config.count);

    assert(pool->index_check == 0);

    pool->index_check = (bool*)calloc(pool->config.count, sizeof(bool));
    assert(pool->index_check != 0);
  }
  assert(pool->items != 0);
}

bool kl_binary_pool_init(KL_BinaryPool* pool, KL_BinaryPoolConfig cfg) {
  if (pool->initialized) return true;
  assert(cfg.count > 0);
  assert(cfg.element_size > 0);

  if (!kl_mutex_init(&pool->lock)) {
    return false;
  }

  kl_int64_list_init(&pool->free_indices, (KL_ListConfig){
    .capacity = cfg.count,
    .allocator = cfg.allocator,
  });
  
  kl_int64_list_init(&pool->taken_indices, (KL_ListConfig){
    .capacity = cfg.count,
    .allocator = cfg.allocator,
  });

  pool->config = cfg;
  pool->items = 0;
  pool->index_check = 0;
  pool->initialized = true;

  return true;
}

void kl_binary_pool_destroy(KL_BinaryPool* pool) {
  assert(pool->initialized);

  kl_mutex_lock(&pool->lock);
  
  kl_int64_list_destroy(&pool->free_indices);
  kl_int64_list_destroy(&pool->taken_indices);

  if (pool->items != 0) {
    free(pool->items);
    pool->items = 0;

    assert(pool->index_check != 0);
    free(pool->index_check);
    pool->index_check = 0;
  }

  assert(pool->items == 0);
  assert(pool->index_check == 0);

  kl_mutex_unlock(&pool->lock);
  kl_mutex_destroy(&pool->lock);

  memset(pool, 0, sizeof(KL_BinaryPool));

}

void kl_binary_pool_reset(KL_BinaryPool* pool) {
  assert(pool->initialized);
  if (pool->items == 0) return;

  kl_mutex_lock(&pool->lock);
  
  memset(pool->items, 0, pool->config.count * pool_stride(pool));
  memset(pool->index_check, 0, pool->config.count * sizeof(bool));
  kl_int64_list_reset(&pool->free_indices);
  kl_int64_list_reset(&pool->taken_indices);
  fill_free_indices(&pool->free_indices, pool->config.count);
  
  kl_mutex_unlock(&pool->lock);
}

uint8_t* kl_binary_pool_acquire_slot(KL_BinaryPool* pool, int64_t* out_idx) {
  assert(pool->initialized);

  kl_mutex_lock(&pool->lock);
  
  ensure_slots(pool);

  *out_idx = -1;

  if (pool->free_indices.length <= 0) {
    kl_mutex_unlock(&pool->lock);
    return 0;
  }

  kl_int64_list_pop_at(&pool->free_indices, 0, out_idx);
  int64_t idx = *out_idx;
  assert(idx >= 0);
  assert(idx < pool->config.count);

  assert(pool->items != 0);

  pool->index_check[idx] = true;

  uint8_t* ptr = &pool->items[idx * pool_stride(pool)];

  kl_int64_list_push(&pool->taken_indices, idx);

  kl_mutex_unlock(&pool->lock);

  return ptr;
}

uint8_t* kl_binary_pool_get_slot(KL_BinaryPool* pool, int64_t idx) {
  assert(pool->initialized);
  if (pool->items == 0) return 0;
  assert(idx >= 0);
  assert(idx < pool->config.count);

  return &pool->items[idx * pool_stride(pool)];
}

void kl_binary_pool_release_slot(KL_BinaryPool* pool, int64_t idx) {
  assert(pool->initialized);
  if (pool->items == 0) return;

  kl_mutex_lock(&pool->lock);

  assert(pool->index_check != 0);

  assert(idx >= 0);
  assert(idx < pool->config.count);

  if (!pool->index_check[idx]) {
    kl_mutex_unlock(&pool->lock);
    return;
  }

  pool->index_check[idx] = false;
  kl_int64_list_push(&pool->free_indices, idx);

  int64_t idx_idx = kl_int64_list_index_of(&pool->taken_indices, idx);
  assert(idx_idx >= 0);
  kl_int64_list_pop_at(&pool->taken_indices, idx_idx, 0);

  kl_mutex_unlock(&pool->lock);
}
