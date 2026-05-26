#ifndef KL_BINARY_SPARSE_SET_H
#define KL_BINARY_SPARSE_SET_H

#include <stdbool.h>
#include <stdint.h>
#include <kl/memory/allocator.h>

// A sparse set that stores fixed-size elements inline in a contiguous byte
// buffer. Unlike SEB_blob_SparseSet (which stores SEB_Blob pointers), this
// embeds the actual bytes directly — no pointer chasing during iteration.

typedef struct {
  int64_t sparse_capacity;
  int64_t dense_capacity;
  int64_t element_size;
  int64_t element_alignment;
  KL_Allocator* allocator; // optional
} KL_BinarySparseSetConfig;

typedef struct {
  KL_BinarySparseSetConfig config;
  bool initialized;
  int64_t length;
  int64_t sparse_capacity;
  int64_t dense_capacity;
  int64_t stride;          // element_size, cached for iteration
  int64_t alignment;       // element_alignment, cached

  int64_t* sparse;         // entity_id -> dense index  (-1 = absent)
  int64_t* indices;        // dense index -> entity_id
  uint8_t* values;         // dense byte buffer (length * stride bytes)

  KL_Allocation _sparse_ref;
  KL_Allocation _indices_ref;
  KL_Allocation _values_ref;
} KL_BinarySparseSet;

void  kl_binary_sparseset_init(KL_BinarySparseSet* set, KL_BinarySparseSetConfig cfg);
void  kl_binary_sparseset_destroy(KL_BinarySparseSet* set);
void  kl_binary_sparseset_reset(KL_BinarySparseSet* set);

// Insert or overwrite. `data` must point to at least `stride` bytes.
// If `data` is NULL, the slot is zero-initialized.
// Returns pointer into the dense buffer where the element now lives.
void* kl_binary_sparseset_set(KL_BinarySparseSet* set, int64_t id, const void* data);

// Returns pointer into the dense buffer, or NULL.
void* kl_binary_sparseset_get(KL_BinarySparseSet* set, int64_t id);

bool  kl_binary_sparseset_has(KL_BinarySparseSet* set, int64_t id);

// Swap-and-pop removal. Returns true if the element existed.
bool  kl_binary_sparseset_remove(KL_BinarySparseSet* set, int64_t id);

// Get pointer to element at dense index `i`. Useful for iteration.
void* kl_binary_sparseset_at(KL_BinarySparseSet* set, int64_t i); 

#endif
