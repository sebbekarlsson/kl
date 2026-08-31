/**
* A thread-safe structure for acquiring and recycling memory.
* The capacity is fixed by default; set `grow_when_full` to let the pool add
* another `count` slots whenever it runs out. Growth appends a new chunk rather
* than reallocating, so slot pointers and handles stay valid across it.
**/


#ifndef KL_BINARY_POOL_H
#define KL_BINARY_POOL_H

#include <kl/templates/list/int64_list.h>
#include <kl/memory/allocator.h>
#include <kl/threads/mutex.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
  // Initial slot count. With `grow_when_full` it is also the chunk size: every
  // growth step adds another `count` slots.
  int64_t count;
  int64_t element_size;
  int64_t element_alignment;

  // When true, an acquire on a full pool allocates a new chunk of `count` slots
  // instead of failing. Existing slot pointers/handles are unaffected.
  bool grow_when_full;

  KL_Allocator* allocator; // optional
} KL_BinaryPoolConfig;

// Slot indices are 32-bit inside handles, so the pool can never hold more.
#define KL_BINARY_POOL_MAX_CAPACITY ((int64_t)1 << 32)

typedef struct {
  bool initialized;
  KL_BinaryPoolConfig config;

  // Slot storage is chunked: chunk i holds slots [i*count, (i+1)*count). A
  // fixed pool has exactly one chunk; a growing pool appends chunks. Chunks are
  // never moved, so a slot's address is stable for the pool's lifetime.
  uint8_t** chunks;
  KL_Allocation chunks_allocation;
  int64_t chunk_count;
  // Total slots across all chunks (== chunk_count * config.count).
  int64_t capacity;

  KL_int64_List free_indices;
  KL_int64_List taken_indices;

  // For each slot: -1 if free, otherwise the position of that slot's index
  // inside `taken_indices`. Doubles as the "is taken?" check and as a
  // direct pointer for O(1) swap-and-pop on release.
  int64_t* taken_pos;
  KL_Allocation taken_pos_allocation;

  // For each slot: a version counter bumped on every acquire. Encoded into the
  // high bits of a handle so a stale handle (whose slot was released and later
  // recycled into a different occupant) can be detected and rejected.
  int64_t* generations;
  KL_Allocation generations_allocation;

  KL_Mutex lock;
} KL_BinaryPool;

bool kl_binary_pool_init(KL_BinaryPool* pool, KL_BinaryPoolConfig cfg);

void kl_binary_pool_destroy(KL_BinaryPool* pool);

// Marks every slot free and zeroes slot memory. Capacity gained through
// growth is kept, not released.
void kl_binary_pool_reset(KL_BinaryPool* pool);

// Total number of slots currently backed by storage (0 before the first
// acquire). Grows in steps of `config.count` when `grow_when_full` is set.
int64_t kl_binary_pool_capacity(KL_BinaryPool* pool);

// --- Index-addressed API -----------------------------------------------------
// Slots are addressed by a bare index. The index stays valid only while the
// caller knows the slot has not been released; after a release+reacquire the
// same index may name a different occupant. Use the handle API below when you
// need to hold a reference across possible reuse.

uint8_t* kl_binary_pool_acquire_slot(KL_BinaryPool* pool, int64_t* out_idx);

uint8_t* kl_binary_pool_get_slot(KL_BinaryPool* pool, int64_t idx);

bool kl_binary_pool_has_slot(KL_BinaryPool* pool, int64_t idx);

void kl_binary_pool_release_slot(KL_BinaryPool* pool, int64_t idx);

// --- Handle-addressed API ----------------------------------------------------
// A handle packs the slot index in the low 32 bits and the slot's generation in
// the next 31 bits; the top bit is reserved 0 so handles are always
// non-negative (and -1 stays a unique "invalid" sentinel). The generation is
// bumped on every acquire, so a handle stops resolving once its slot is released
// or recycled — a stale handle can't address the wrong occupant.
//
// The generation wraps modulo 2^31 instead of overflowing. A wrap can only
// alias a handle that was already released (a live handle sits on a taken slot,
// whose generation is frozen), and only after that exact slot is re-acquired
// 2^31 times — an accepted trade for keeping handles a cheap int64.
#define KL_BINARY_POOL_HANDLE_INDEX_BITS 32
#define KL_BINARY_POOL_HANDLE_INDEX_MASK ((int64_t)0xFFFFFFFF)
#define KL_BINARY_POOL_HANDLE_GENERATION_MASK ((int64_t)0x7FFFFFFF)

static inline int64_t kl_binary_pool_handle_index(int64_t handle) {
  return handle & KL_BINARY_POOL_HANDLE_INDEX_MASK;
}

static inline int64_t kl_binary_pool_handle_generation(int64_t handle) {
  return ((int64_t)((uint64_t)handle >> KL_BINARY_POOL_HANDLE_INDEX_BITS))
       & KL_BINARY_POOL_HANDLE_GENERATION_MASK;
}

// Acquire a slot and return a versioned handle (>= 0), or -1 if the pool is
// full (and cannot or may not grow). If out_ptr is non-null it receives the
// slot's storage pointer.
int64_t kl_binary_pool_acquire(KL_BinaryPool* pool, uint8_t** out_ptr);

// Map a handle to its live slot index, or -1 if the handle is stale (slot freed
// or recycled into a newer occupant). Useful to bridge into the index API.
int64_t kl_binary_pool_resolve(KL_BinaryPool* pool, int64_t handle);

// Handle-addressed counterparts of get/has/release. They validate the
// generation, so a stale handle reads as absent / releases nothing.
uint8_t* kl_binary_pool_get(KL_BinaryPool* pool, int64_t handle);

bool kl_binary_pool_has(KL_BinaryPool* pool, int64_t handle);

void kl_binary_pool_release(KL_BinaryPool* pool, int64_t handle);


#endif
