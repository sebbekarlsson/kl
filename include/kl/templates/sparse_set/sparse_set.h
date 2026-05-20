/*
This file was automatically generated and must not be modified directly.

Generated at: 2026-05-21 00:00:53
*/


#include <stdbool.h>
#include <stdint.h>
#include <kl/memory/allocator.h>
#include <kl/templates/sparse_set/shared.h>
#include <kl/templates/x__shared.h>
#include <kl/templates/sparse_set/dummy.h>
#include <assert.h>
#include <string.h>


#define KL_DEFINE_SPARSE_SET(NAME, T)                                                    \
  typedef struct {                                                                       \
    KL_SparseSetConfig config;                                                           \
    bool initialized;                                                                    \
    int64_t length;                                                                      \
    int64_t sparse_capacity;                                                             \
    int64_t dense_capacity;                                                              \
    int64_t* sparse;                                                                     \
    int64_t* indices;                                                                    \
    T* values;                                                                           \
    KL_Allocation _sparse_ref;                                                           \
    KL_Allocation _indices_ref;                                                          \
    KL_Allocation _values_ref;                                                           \
  } KL_##NAME##_SparseSet;                                                               \
  void kl_##NAME##_sparse_set_init(KL_##NAME##_SparseSet* set, KL_SparseSetConfig cfg);  \
  void kl_##NAME##_sparse_set_destroy(KL_##NAME##_SparseSet* set);                       \
  void kl_##NAME##_sparse_set_reset(KL_##NAME##_SparseSet* set);                         \
  T* kl_##NAME##_sparse_set_set(KL_##NAME##_SparseSet* set, int64_t id, T value);        \
  T* kl_##NAME##_sparse_set_get(KL_##NAME##_SparseSet* set, int64_t id);                 \
  bool kl_##NAME##_sparse_set_has(KL_##NAME##_SparseSet* set, int64_t id);               \
  bool kl_##NAME##_sparse_set_remove(KL_##NAME##_SparseSet* set, int64_t id);



#define KL_IMPL_SPARSE_SET(NAME, T)                                                                                                                              \
  static void kl_##NAME##_sparse_set_ensure_alloc(KL_##NAME##_SparseSet* set) {                                                                                  \
    if (set->sparse != 0) return;                                                                                                                                \
    KL_Allocator* alloc = set->config.allocator;                                                                                                                 \
    set->sparse = (int64_t*)kl_allocator_malloc_tracked_aligned(alloc, set->sparse_capacity * (int64_t)sizeof(int64_t), _Alignof(int64_t), &set->_sparse_ref);   \
    memset(set->sparse, 0xFF, (size_t)(set->sparse_capacity * (int64_t)sizeof(int64_t)));                                                                        \
    set->indices = (int64_t*)kl_allocator_malloc_tracked_aligned(alloc, set->dense_capacity * (int64_t)sizeof(int64_t), _Alignof(int64_t), &set->_indices_ref);  \
    set->values = (T*)kl_allocator_malloc_tracked_aligned(alloc, set->dense_capacity * (int64_t)sizeof(T), _Alignof(T), &set->_values_ref);                      \
  }                                                                                                                                                              \
  void kl_##NAME##_sparse_set_init(KL_##NAME##_SparseSet* set, KL_SparseSetConfig cfg) {                                                                         \
    if (set->initialized) return;                                                                                                                                \
    assert(cfg.sparse_capacity > 0);                                                                                                                             \
    assert(cfg.dense_capacity > 0);                                                                                                                              \
    assert(cfg.allocator != 0);                                                                                                                                  \
    set->config = cfg;                                                                                                                                           \
    set->length = 0;                                                                                                                                             \
    set->sparse_capacity = cfg.sparse_capacity;                                                                                                                  \
    set->dense_capacity = cfg.dense_capacity;                                                                                                                    \
    set->sparse = 0;                                                                                                                                             \
    kl_##NAME##_sparse_set_ensure_alloc(set);                                                                                                                    \
    set->initialized = true;                                                                                                                                     \
  }                                                                                                                                                              \
  void kl_##NAME##_sparse_set_destroy(KL_##NAME##_SparseSet* set) {                                                                                              \
    if (!set->initialized) return;                                                                                                                               \
    set->sparse = 0;                                                                                                                                             \
    set->indices = 0;                                                                                                                                            \
    set->values = 0;                                                                                                                                             \
    set->_sparse_ref = (KL_Allocation){0};                                                                                                                       \
    set->_indices_ref = (KL_Allocation){0};                                                                                                                      \
    set->_values_ref = (KL_Allocation){0};                                                                                                                       \
    set->length = 0;                                                                                                                                             \
    set->sparse_capacity = 0;                                                                                                                                    \
    set->dense_capacity = 0;                                                                                                                                     \
    set->initialized = false;                                                                                                                                    \
  }                                                                                                                                                              \
  void kl_##NAME##_sparse_set_reset(KL_##NAME##_SparseSet* set) {                                                                                                \
    if (!set->initialized) return;                                                                                                                               \
    set->length = 0;                                                                                                                                             \
    if (set->sparse) {                                                                                                                                           \
      memset(set->sparse, 0xFF, (size_t)(set->sparse_capacity * (int64_t)sizeof(int64_t)));                                                                      \
    }                                                                                                                                                            \
  }                                                                                                                                                              \
  T* kl_##NAME##_sparse_set_set(KL_##NAME##_SparseSet* set, int64_t id, T value) {                                                                               \
    assert(set->initialized);                                                                                                                                    \
    assert(id >= 0);                                                                                                                                             \
    kl_##NAME##_sparse_set_ensure_alloc(set);                                                                                                                    \
    KL_Allocator* alloc = set->config.allocator;                                                                                                                 \
    if (id >= set->sparse_capacity) {                                                                                                                            \
      int64_t new_sparse_cap = set->sparse_capacity * 2;                                                                                                         \
      while (new_sparse_cap <= id) new_sparse_cap *= 2;                                                                                                          \
      int64_t old_size = set->sparse_capacity * (int64_t)sizeof(int64_t);                                                                                        \
      int64_t new_size = new_sparse_cap * (int64_t)sizeof(int64_t);                                                                                              \
      set->sparse = (int64_t*)kl_allocator_realloc_aligned(alloc, new_size, _Alignof(int64_t), &set->_sparse_ref);                                               \
      memset((uint8_t*)set->sparse + old_size, 0xFF, (size_t)(new_size - old_size));                                                                             \
      set->sparse_capacity = new_sparse_cap;                                                                                                                     \
    }                                                                                                                                                            \
    if (set->sparse[id] != -1) {                                                                                                                                 \
      set->values[set->sparse[id]] = value;                                                                                                                      \
      return &set->values[set->sparse[id]];                                                                                                                      \
    }                                                                                                                                                            \
    if (set->length >= set->dense_capacity) {                                                                                                                    \
      int64_t new_dense_cap = set->dense_capacity * 2;                                                                                                           \
      set->indices = (int64_t*)kl_allocator_realloc_aligned(alloc, new_dense_cap * (int64_t)sizeof(int64_t), _Alignof(int64_t), &set->_indices_ref);             \
      set->values = (T*)kl_allocator_realloc_aligned(alloc, new_dense_cap * (int64_t)sizeof(T), _Alignof(T), &set->_values_ref);                                 \
      set->dense_capacity = new_dense_cap;                                                                                                                       \
    }                                                                                                                                                            \
    set->values[set->length] = value;                                                                                                                            \
    set->indices[set->length] = id;                                                                                                                              \
    set->sparse[id] = set->length;                                                                                                                               \
    set->length++;                                                                                                                                               \
    return &set->values[set->sparse[id]];                                                                                                                        \
  }                                                                                                                                                              \
  T* kl_##NAME##_sparse_set_get(KL_##NAME##_SparseSet* set, int64_t id) {                                                                                        \
    assert(set->initialized);                                                                                                                                    \
    if (!set->sparse) return 0;                                                                                                                                  \
    if (id < 0 || id >= set->sparse_capacity) return 0;                                                                                                          \
    int64_t pos = set->sparse[id];                                                                                                                               \
    if (pos == -1) return 0;                                                                                                                                     \
    return &set->values[pos];                                                                                                                                    \
  }                                                                                                                                                              \
  bool kl_##NAME##_sparse_set_has(KL_##NAME##_SparseSet* set, int64_t id) {                                                                                      \
    assert(set->initialized);                                                                                                                                    \
    if (!set->sparse) return false;                                                                                                                              \
    return id >= 0 && id < set->sparse_capacity && set->sparse[id] != -1;                                                                                        \
  }                                                                                                                                                              \
  bool kl_##NAME##_sparse_set_remove(KL_##NAME##_SparseSet* set, int64_t id) {                                                                                   \
    assert(set->initialized);                                                                                                                                    \
    if (!kl_##NAME##_sparse_set_has(set, id)) return false;                                                                                                      \
    int64_t dense_pos = set->sparse[id];                                                                                                                         \
    int64_t last_pos = set->length - 1;                                                                                                                          \
    if (dense_pos != last_pos) {                                                                                                                                 \
      int64_t last_id = set->indices[last_pos];                                                                                                                  \
      set->values[dense_pos] = set->values[last_pos];                                                                                                            \
      set->indices[dense_pos] = last_id;                                                                                                                         \
      set->sparse[last_id] = dense_pos;                                                                                                                          \
    }                                                                                                                                                            \
    set->sparse[id] = -1;                                                                                                                                        \
    set->length--;                                                                                                                                               \
    return true;                                                                                                                                                 \
  }

/* generated */
