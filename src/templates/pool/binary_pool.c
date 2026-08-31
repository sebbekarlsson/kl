#include <kl/templates/pool/binary_pool.h>
#include <kl/macros.h>
#include <kl/memory/alloc.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static void push_free_range(KL_int64_List* indices, int64_t from, int64_t to) {
  for (int64_t i = from; i < to; i++) kl_int64_list_push(indices, i);
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

// Growable bookkeeping arrays (chunk table, taken_pos, generations). Same
// scheme as the list template: a KL_Allocation handle rides along so the
// allocator can realloc in place / free on destroy; the heap path uses
// kl_aligned_realloc. Contents below the old size are preserved.
static uint8_t* pool_realloc(KL_BinaryPool* pool, void* ptr, KL_Allocation* allocation,
                             int64_t new_size, size_t alignment) {
  uint8_t* out = (pool->config.allocator != 0)
    ? kl_allocator_realloc_aligned(pool->config.allocator, new_size, alignment, allocation)
    : (uint8_t*)kl_aligned_realloc(ptr, (size_t)new_size, alignment);
  assert(out != 0);
  return out;
}

static void pool_free_realloced(KL_BinaryPool* pool, void* ptr, KL_Allocation* allocation) {
  if (pool->config.allocator == 0) {
    if (ptr != 0) free(ptr);
  } else if (allocation->size > 0 && pool->config.allocator->initialized) {
    kl_allocator_free_allocation(pool->config.allocator, *allocation);
  }
  *allocation = (KL_Allocation){0};
}

static inline uint8_t* slot_ptr(const KL_BinaryPool* pool, int64_t idx) {
  assert(idx >= 0);
  assert(idx < pool->capacity);
  int64_t chunk_size = pool->config.count;
  return &pool->chunks[idx / chunk_size][(idx % chunk_size) * pool_stride(pool)];
}

// Append one chunk of `config.count` slots. Caller must hold pool->lock.
// Returns false (leaving the pool untouched) if the handle index space is
// exhausted.
static bool add_chunk(KL_BinaryPool* pool) {
  int64_t chunk_size = pool->config.count;
  int64_t old_capacity = pool->capacity;
  int64_t new_capacity = old_capacity + chunk_size;
  if (new_capacity > KL_BINARY_POOL_MAX_CAPACITY) return false;

  int64_t stride = pool_stride(pool);

  pool->chunks = (uint8_t**)pool_realloc(
    pool, pool->chunks, &pool->chunks_allocation,
    (pool->chunk_count + 1) * (int64_t)sizeof(uint8_t*), _Alignof(uint8_t*));
  pool->taken_pos = (int64_t*)pool_realloc(
    pool, pool->taken_pos, &pool->taken_pos_allocation,
    new_capacity * (int64_t)sizeof(int64_t), _Alignof(int64_t));
  pool->generations = (int64_t*)pool_realloc(
    pool, pool->generations, &pool->generations_allocation,
    new_capacity * (int64_t)sizeof(int64_t), _Alignof(int64_t));

  uint8_t* chunk = pool_alloc(pool, chunk_size * stride, (size_t)pool->config.element_alignment);
  memset(chunk, 0, (size_t)(chunk_size * stride));
  pool->chunks[pool->chunk_count] = chunk;

  // 0xff bytes → int64_t -1 (two's complement); a zero byte would mean
  // "position 0", which is a valid taken position.
  memset(&pool->taken_pos[old_capacity], 0xff, (size_t)chunk_size * sizeof(int64_t));
  // Generation 0 is the "never acquired" baseline; the first acquire of each
  // slot bumps it to 1.
  memset(&pool->generations[old_capacity], 0, (size_t)chunk_size * sizeof(int64_t));

  push_free_range(&pool->free_indices, old_capacity, new_capacity);

  pool->chunk_count += 1;
  pool->capacity = new_capacity;
  return true;
}

static void ensure_slots(KL_BinaryPool* pool) {
  if (pool->chunk_count == 0) {
    assert(pool->free_indices.length == 0);
    bool ok = add_chunk(pool);
    assert(ok);
    (void)ok;
  }
  assert(pool->chunks != 0);
  assert(pool->capacity > 0);
}

bool kl_binary_pool_init(KL_BinaryPool* pool, KL_BinaryPoolConfig cfg) {
  if (pool->initialized) return true;
  assert(cfg.count > 0);
  assert(cfg.count <= KL_BINARY_POOL_MAX_CAPACITY);
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
  pool->chunks = 0;
  pool->chunks_allocation = (KL_Allocation){0};
  pool->chunk_count = 0;
  pool->capacity = 0;
  pool->taken_pos = 0;
  pool->taken_pos_allocation = (KL_Allocation){0};
  pool->generations = 0;
  pool->generations_allocation = (KL_Allocation){0};
  pool->initialized = true;

  return true;
}

void kl_binary_pool_destroy(KL_BinaryPool* pool) {
  assert(pool->initialized);

  kl_mutex_lock(&pool->lock);
  
  kl_int64_list_destroy(&pool->free_indices);
  kl_int64_list_destroy(&pool->taken_indices);

  if (pool->chunks != 0) {
    for (int64_t i = 0; i < pool->chunk_count; i++) {
      pool_free(pool, pool->chunks[i]);
      pool->chunks[i] = 0;
    }
    pool_free_realloced(pool, pool->chunks, &pool->chunks_allocation);
    pool->chunks = 0;
    pool->chunk_count = 0;
    pool->capacity = 0;

    assert(pool->taken_pos != 0);
    pool_free_realloced(pool, pool->taken_pos, &pool->taken_pos_allocation);
    pool->taken_pos = 0;

    assert(pool->generations != 0);
    pool_free_realloced(pool, pool->generations, &pool->generations_allocation);
    pool->generations = 0;
  }

  assert(pool->chunks == 0);
  assert(pool->taken_pos == 0);
  assert(pool->generations == 0);

  kl_mutex_unlock(&pool->lock);
  kl_mutex_destroy(&pool->lock);

  memset(pool, 0, sizeof(KL_BinaryPool));

}

void kl_binary_pool_reset(KL_BinaryPool* pool) {
  assert(pool->initialized);
  if (pool->chunks == 0) return;

  kl_mutex_lock(&pool->lock);

  int64_t chunk_bytes = pool->config.count * pool_stride(pool);
  for (int64_t i = 0; i < pool->chunk_count; i++) {
    memset(pool->chunks[i], 0, (size_t)chunk_bytes);
  }
  memset(pool->taken_pos, 0xff, (size_t)pool->capacity * sizeof(int64_t));
  // Deliberately leave `generations` untouched: keeping the counters monotonic
  // across a reset means a handle minted before the reset can never collide with
  // one minted after it.
  kl_int64_list_reset(&pool->free_indices);
  kl_int64_list_reset(&pool->taken_indices);
  push_free_range(&pool->free_indices, 0, pool->capacity);
  
  kl_mutex_unlock(&pool->lock);
}

int64_t kl_binary_pool_capacity(KL_BinaryPool* pool) {
  assert(pool->initialized);

  kl_mutex_lock(&pool->lock);
  int64_t capacity = pool->capacity;
  kl_mutex_unlock(&pool->lock);

  return capacity;
}

static inline int64_t handle_encode(int64_t index, int64_t generation) {
  return (int64_t)((uint64_t)(generation & KL_BINARY_POOL_HANDLE_GENERATION_MASK) << KL_BINARY_POOL_HANDLE_INDEX_BITS)
       | (index & KL_BINARY_POOL_HANDLE_INDEX_MASK);
}

// Acquire a slot. Caller must hold pool->lock. Returns the slot pointer and
// writes the slot index to *out_idx, or returns 0 (and *out_idx = -1) when full
// and growth is either disabled or no longer possible.
static uint8_t* acquire_locked(KL_BinaryPool* pool, int64_t* out_idx) {
  ensure_slots(pool);

  *out_idx = -1;

  if (pool->free_indices.length <= 0) {
    if (!pool->config.grow_when_full) return 0;
    if (!add_chunk(pool)) return 0;
    assert(pool->free_indices.length > 0);
  }

  int64_t idx = -1;
  kl_int64_list_pop(&pool->free_indices, &idx);
  assert(idx >= 0);
  assert(idx < pool->capacity);

  pool->taken_pos[idx] = pool->taken_indices.length;
  kl_int64_list_push(&pool->taken_indices, idx);

  // Bump the generation so any handle referring to a previous occupant of this
  // slot no longer resolves. Wrap modulo the field width (rather than letting it
  // overflow) so the stored counter always matches what a handle decodes to.
  pool->generations[idx] = (pool->generations[idx] + 1) & KL_BINARY_POOL_HANDLE_GENERATION_MASK;

  *out_idx = idx;
  return slot_ptr(pool, idx);
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
  assert(idx >= 0);

  // The chunk table may be reallocated by a concurrent grow, so read it under
  // the lock. The slot memory itself never moves.
  kl_mutex_lock(&pool->lock);
  uint8_t* ptr = 0;
  if (pool->chunks != 0) {
    assert(idx < pool->capacity);
    ptr = slot_ptr(pool, idx);
  }
  kl_mutex_unlock(&pool->lock);

  return ptr;
}

bool kl_binary_pool_has_slot(KL_BinaryPool* pool, int64_t idx) {
  assert(pool->initialized);
  if (idx < 0) return false;

  kl_mutex_lock(&pool->lock);
  bool has = pool->taken_pos != 0 && idx < pool->capacity && pool->taken_pos[idx] >= 0;
  kl_mutex_unlock(&pool->lock);
  return has;
}

// Release a taken slot. Caller must hold pool->lock. No-op if idx is free.
static void release_locked(KL_BinaryPool* pool, int64_t idx) {
  assert(pool->taken_pos != 0);
  assert(idx >= 0);
  assert(idx < pool->capacity);

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
  if (pool->chunks == 0) return;

  kl_mutex_lock(&pool->lock);
  release_locked(pool, idx);
  kl_mutex_unlock(&pool->lock);
}

// Resolve a handle to its live slot index, or -1 if stale. Caller holds the lock.
static int64_t resolve_locked(KL_BinaryPool* pool, int64_t handle) {
  if (pool->taken_pos == 0) return -1;

  int64_t idx = kl_binary_pool_handle_index(handle);
  if (idx < 0 || idx >= pool->capacity) return -1;
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
  uint8_t* ptr = (idx >= 0) ? slot_ptr(pool, idx) : 0;
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
  if (pool->chunks == 0) return;

  kl_mutex_lock(&pool->lock);
  int64_t idx = resolve_locked(pool, handle);
  if (idx >= 0) release_locked(pool, idx);
  kl_mutex_unlock(&pool->lock);
}
