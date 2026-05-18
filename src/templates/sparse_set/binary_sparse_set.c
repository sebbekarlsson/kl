#include <kl/templates/sparse_set/binary_sparse_set.h>
#include <assert.h>
#include <string.h>

void kl_binary_sparseset_init(KL_BinarySparseSet* set, KL_BinarySparseSetConfig cfg) {
  if (set->initialized) return;
  assert(cfg.sparse_capacity > 0);
  assert(cfg.dense_capacity > 0);
  assert(cfg.element_size > 0);
  assert(cfg.allocator != 0);

  set->config = cfg;
  set->length = 0;
  set->sparse_capacity = cfg.sparse_capacity;
  set->dense_capacity = cfg.dense_capacity;
  set->stride = cfg.element_size;
  set->alignment = cfg.element_alignment > 0 ? cfg.element_alignment : 1;

  set->sparse = (int64_t*)kl_allocator_malloc_tracked_aligned(
    cfg.allocator,
    cfg.sparse_capacity * (int64_t)sizeof(int64_t),
    _Alignof(int64_t),
    &set->_sparse_ref
  );
  memset(set->sparse, 0xFF, (size_t)(cfg.sparse_capacity * (int64_t)sizeof(int64_t)));

  set->indices = (int64_t*)kl_allocator_malloc_tracked_aligned(
    cfg.allocator,
    cfg.dense_capacity * (int64_t)sizeof(int64_t),
    _Alignof(int64_t),
    &set->_indices_ref
  );

  set->values = kl_allocator_malloc_tracked_aligned(
    cfg.allocator,
    cfg.dense_capacity * set->stride,
    set->alignment,
    &set->_values_ref
  );

  set->initialized = true;
}

void kl_binary_sparseset_destroy(KL_BinarySparseSet* set) {
  if (!set->initialized) return;
  set->sparse = 0;
  set->indices = 0;
  set->values = 0;
  set->_sparse_ref = (KL_Allocation){0};
  set->_indices_ref = (KL_Allocation){0};
  set->_values_ref = (KL_Allocation){0};
  set->length = 0;
  set->sparse_capacity = 0;
  set->dense_capacity = 0;
  set->initialized = false;
}

void kl_binary_sparseset_reset(KL_BinarySparseSet* set) {
  if (!set->initialized) return;
  set->length = 0;
  memset(set->sparse, 0xFF, (size_t)(set->sparse_capacity * (int64_t)sizeof(int64_t)));
}

void* kl_binary_sparseset_set(KL_BinarySparseSet* set, int64_t id, const void* data) {
  assert(set->initialized);
  assert(id >= 0);

  KL_Allocator* alloc = set->config.allocator;

  // Grow sparse array if needed.
  if (id >= set->sparse_capacity) {
    int64_t new_sparse_cap = set->sparse_capacity * 2;
    while (new_sparse_cap <= id) new_sparse_cap *= 2;

    int64_t old_size = set->sparse_capacity * (int64_t)sizeof(int64_t);
    int64_t new_size = new_sparse_cap * (int64_t)sizeof(int64_t);

    set->sparse = (int64_t*)kl_allocator_realloc_aligned(alloc, new_size, _Alignof(int64_t), &set->_sparse_ref);
    memset((uint8_t*)set->sparse + old_size, 0xFF, (size_t)(new_size - old_size));
    set->sparse_capacity = new_sparse_cap;
  }

  // Overwrite existing entry.
  if (set->sparse[id] != -1) {
    void* dest = set->values + set->sparse[id] * set->stride;
    if (data) {
      memcpy(dest, data, (size_t)set->stride);
    } else {
      memset(dest, 0, (size_t)set->stride);
    }
    return dest;
  }

  // Grow dense arrays if needed.
  if (set->length >= set->dense_capacity) {
    int64_t new_dense_cap = set->dense_capacity * 2;
    set->indices = (int64_t*)kl_allocator_realloc_aligned(
      alloc,
      new_dense_cap * (int64_t)sizeof(int64_t),
      _Alignof(int64_t),
      &set->_indices_ref
    );
    set->values = (uint8_t*)kl_allocator_realloc_aligned(
      alloc,
      new_dense_cap * set->stride,
      set->alignment,
      &set->_values_ref
    );
    set->dense_capacity = new_dense_cap;
  }

  // Write into the next dense slot.
  void* dest = set->values + set->length * set->stride;
  if (data) {
    memcpy(dest, data, (size_t)set->stride);
  } else {
    memset(dest, 0, (size_t)set->stride);
  }
  set->indices[set->length] = id;
  set->sparse[id] = set->length;
  set->length++;

  return dest;
}

void* kl_binary_sparseset_get(KL_BinarySparseSet* set, int64_t id) {
  assert(set->initialized);
  if (id < 0 || id >= set->sparse_capacity) return 0;
  int64_t pos = set->sparse[id];
  if (pos == -1) return 0;
  return set->values + pos * set->stride;
}

bool kl_binary_sparseset_has(KL_BinarySparseSet* set, int64_t id) {
  assert(set->initialized);
  return id >= 0 && id < set->sparse_capacity && set->sparse[id] != -1;
}

bool kl_binary_sparseset_remove(KL_BinarySparseSet* set, int64_t id) {
  assert(set->initialized);
  if (!kl_binary_sparseset_has(set, id)) return false;

  int64_t dense_pos = set->sparse[id];
  int64_t last_pos = set->length - 1;

  if (dense_pos != last_pos) {
    int64_t last_id = set->indices[last_pos];

    // Swap last element's bytes into the removed slot.
    memcpy(
      set->values + dense_pos * set->stride,
      set->values + last_pos * set->stride,
      (size_t)set->stride
    );
    set->indices[dense_pos] = last_id;
    set->sparse[last_id] = dense_pos;
  }

  set->sparse[id] = -1;
  set->length--;
  return true;
}

void* kl_binary_sparseset_at(KL_BinarySparseSet* set, int64_t i) {
  return set->values + i * set->stride;
}
