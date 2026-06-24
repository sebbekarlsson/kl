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

// Backing storage honours the configured allocator when one is provided, and
// falls back to the heap otherwise. Allocator-owned memory is reclaimed when the
// allocator itself is destroyed, so pool_free only releases the heap fallback.
static uint8_t* pool_alloc(KL_BinaryPool* pool, int64_t size, size_t alignment) {
  uint8_t* ptr = (pool->config.allocator != 0)
    ? kl_allocator_malloc_aligned(pool->config.allocator, size, alignment)
    : (uint8_t*)kl_aligned_alloc((size_t)size, alignment);
  assert(ptr != 0);
  return ptr;
}

static void pool_free(KL_BinaryPool* pool, void* ptr) {
  if (pool->config.allocator == 0 && ptr != 0) free(ptr);
}

static void ensure_slots(KL_BinaryPool* pool) {
  if (pool->items == 0) {
    int64_t stride = pool_stride(pool);
    int64_t count = pool->config.count;

    pool->items = pool_alloc(pool, count * stride, (size_t)pool->config.element_alignment);
    assert(pool->free_indices.length == 0);
    fill_free_indices(&pool->free_indices, count);

    assert(pool->taken_pos == 0);
    pool->taken_pos = (int64_t*)pool_alloc(pool, count * (int64_t)sizeof(int64_t), _Alignof(int64_t));
    assert(pool->taken_pos != 0);
    // 0xff bytes → int64_t -1 (two's complement); a zero byte would mean
    // "position 0", which is a valid taken position.
    memset(pool->taken_pos, 0xff, count * sizeof(int64_t));

    assert(pool->generations == 0);
    pool->generations = (int64_t*)pool_alloc(pool, count * (int64_t)sizeof(int64_t), _Alignof(int64_t));
    assert(pool->generations != 0);
    // Generation 0 is the "never acquired" baseline; the first acquire of each
    // slot bumps it to 1.
    memset(pool->generations, 0, count * sizeof(int64_t));
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
  pool->taken_pos = 0;
  pool->generations = 0;
  pool->initialized = true;

  return true;
}

void kl_binary_pool_destroy(KL_BinaryPool* pool) {
  assert(pool->initialized);

  kl_mutex_lock(&pool->lock);
  
  kl_int64_list_destroy(&pool->free_indices);
  kl_int64_list_destroy(&pool->taken_indices);

  if (pool->items != 0) {
    pool_free(pool, pool->items);
    pool->items = 0;

    assert(pool->taken_pos != 0);
    pool_free(pool, pool->taken_pos);
    pool->taken_pos = 0;

    assert(pool->generations != 0);
    pool_free(pool, pool->generations);
    pool->generations = 0;
  }

  assert(pool->items == 0);
  assert(pool->taken_pos == 0);
  assert(pool->generations == 0);

  kl_mutex_unlock(&pool->lock);
  kl_mutex_destroy(&pool->lock);

  memset(pool, 0, sizeof(KL_BinaryPool));

}

void kl_binary_pool_reset(KL_BinaryPool* pool) {
  assert(pool->initialized);
  if (pool->items == 0) return;

  kl_mutex_lock(&pool->lock);
  
  memset(pool->items, 0, pool->config.count * pool_stride(pool));
  memset(pool->taken_pos, 0xff, pool->config.count * sizeof(int64_t));
  // Deliberately leave `generations` untouched: keeping the counters monotonic
  // across a reset means a handle minted before the reset can never collide with
  // one minted after it.
  kl_int64_list_reset(&pool->free_indices);
  kl_int64_list_reset(&pool->taken_indices);
  fill_free_indices(&pool->free_indices, pool->config.count);
  
  kl_mutex_unlock(&pool->lock);
}

static inline int64_t handle_encode(int64_t index, int64_t generation) {
  return (int64_t)((uint64_t)(generation & KL_BINARY_POOL_HANDLE_GENERATION_MASK) << KL_BINARY_POOL_HANDLE_INDEX_BITS)
       | (index & KL_BINARY_POOL_HANDLE_INDEX_MASK);
}

// Acquire a slot. Caller must hold pool->lock. Returns the slot pointer and
// writes the slot index to *out_idx, or returns 0 (and *out_idx = -1) when full.
static uint8_t* acquire_locked(KL_BinaryPool* pool, int64_t* out_idx) {
  ensure_slots(pool);

  *out_idx = -1;

  if (pool->free_indices.length <= 0) return 0;

  int64_t idx = -1;
  kl_int64_list_pop(&pool->free_indices, &idx);
  assert(idx >= 0);
  assert(idx < pool->config.count);

  assert(pool->items != 0);

  pool->taken_pos[idx] = pool->taken_indices.length;
  kl_int64_list_push(&pool->taken_indices, idx);

  // Bump the generation so any handle referring to a previous occupant of this
  // slot no longer resolves. Wrap modulo the field width (rather than letting it
  // overflow) so the stored counter always matches what a handle decodes to.
  pool->generations[idx] = (pool->generations[idx] + 1) & KL_BINARY_POOL_HANDLE_GENERATION_MASK;

  *out_idx = idx;
  return &pool->items[idx * pool_stride(pool)];
}

uint8_t* kl_binary_pool_acquire_slot(KL_BinaryPool* pool, int64_t* out_idx) {
  assert(pool->initialized);

  kl_mutex_lock(&pool->lock);
  uint8_t* ptr = acquire_locked(pool, out_idx);
  kl_mutex_unlock(&pool->lock);

  return ptr;
}

int64_t kl_binary_pool_acquire(KL_BinaryPool* pool, uint8_t** out_ptr) {
  assert(pool->initialized);

  kl_mutex_lock(&pool->lock);

  int64_t idx = -1;
  uint8_t* ptr = acquire_locked(pool, &idx);
  int64_t handle = (ptr != 0) ? handle_encode(idx, pool->generations[idx]) : -1;

  kl_mutex_unlock(&pool->lock);

  if (out_ptr != 0) *out_ptr = ptr;
  return handle;
}

uint8_t* kl_binary_pool_get_slot(KL_BinaryPool* pool, int64_t idx) {
  assert(pool->initialized);
  if (pool->items == 0) return 0;
  assert(idx >= 0);
  assert(idx < pool->config.count);

  return &pool->items[idx * pool_stride(pool)];
}

bool kl_binary_pool_has_slot(KL_BinaryPool* pool, int64_t idx) {
  assert(pool->initialized);
  if (idx < 0 || idx >= pool->config.count) return false;

  kl_mutex_lock(&pool->lock);
  bool has = pool->taken_pos != 0 && pool->taken_pos[idx] >= 0;
  kl_mutex_unlock(&pool->lock);
  return has;
}

// Release a taken slot. Caller must hold pool->lock. No-op if idx is free.
static void release_locked(KL_BinaryPool* pool, int64_t idx) {
  assert(pool->taken_pos != 0);
  assert(idx >= 0);
  assert(idx < pool->config.count);

  int64_t pos = pool->taken_pos[idx];
  if (pos < 0) return;

  // Swap-and-pop on taken_indices: move the trailing entry into `pos`,
  // fix up its taken_pos, then shrink. Reorders taken_indices but the
  // pool only guarantees membership, not order.
  int64_t last_pos = pool->taken_indices.length - 1;
  assert(pos <= last_pos);
  if (pos != last_pos) {
    int64_t last_idx = pool->taken_indices.items[last_pos];
    pool->taken_indices.items[pos] = last_idx;
    pool->taken_pos[last_idx] = pos;
  }
  pool->taken_indices.length -= 1;
  pool->taken_pos[idx] = -1;

  kl_int64_list_push(&pool->free_indices, idx);
}

void kl_binary_pool_release_slot(KL_BinaryPool* pool, int64_t idx) {
  assert(pool->initialized);
  if (pool->items == 0) return;

  kl_mutex_lock(&pool->lock);
  release_locked(pool, idx);
  kl_mutex_unlock(&pool->lock);
}

// Resolve a handle to its live slot index, or -1 if stale. Caller holds the lock.
static int64_t resolve_locked(KL_BinaryPool* pool, int64_t handle) {
  if (pool->taken_pos == 0) return -1;

  int64_t idx = kl_binary_pool_handle_index(handle);
  if (idx < 0 || idx >= pool->config.count) return -1;
  if (pool->taken_pos[idx] < 0) return -1;                                  // slot is free
  if (pool->generations[idx] != kl_binary_pool_handle_generation(handle)) return -1; // recycled

  return idx;
}

int64_t kl_binary_pool_resolve(KL_BinaryPool* pool, int64_t handle) {
  assert(pool->initialized);

  kl_mutex_lock(&pool->lock);
  int64_t idx = resolve_locked(pool, handle);
  kl_mutex_unlock(&pool->lock);

  return idx;
}

uint8_t* kl_binary_pool_get(KL_BinaryPool* pool, int64_t handle) {
  assert(pool->initialized);

  kl_mutex_lock(&pool->lock);
  int64_t idx = resolve_locked(pool, handle);
  uint8_t* ptr = (idx >= 0) ? &pool->items[idx * pool_stride(pool)] : 0;
  kl_mutex_unlock(&pool->lock);

  return ptr;
}

bool kl_binary_pool_has(KL_BinaryPool* pool, int64_t handle) {
  assert(pool->initialized);

  kl_mutex_lock(&pool->lock);
  bool ok = resolve_locked(pool, handle) >= 0;
  kl_mutex_unlock(&pool->lock);

  return ok;
}

void kl_binary_pool_release(KL_BinaryPool* pool, int64_t handle) {
  assert(pool->initialized);
  if (pool->items == 0) return;

  kl_mutex_lock(&pool->lock);
  int64_t idx = resolve_locked(pool, handle);
  if (idx >= 0) release_locked(pool, idx);
  kl_mutex_unlock(&pool->lock);
}
