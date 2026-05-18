#ifndef KL_SPARSE_SET_H
#define KL_SPARSE_SET_H

#include <stdbool.h>
#include <stdint.h>
#include <kl/memory/allocator.h>
#include <kl/templates/sparse_set/shared.h>
#include <kl/templates/x__shared.h>


typedef struct {
  KL_SparseSetConfig config;
  bool initialized;
  int64_t length;
  int64_t sparse_capacity;
  int64_t dense_capacity;
  int64_t* sparse;
  int64_t* indices;
  T_X* values;
  KL_Allocation _sparse_ref;
  KL_Allocation _indices_ref;
  KL_Allocation _values_ref;
} KL_NAME_SparseSet;

void kl_NAME_sparse_set_init(KL_NAME_SparseSet* set, KL_SparseSetConfig cfg);
void kl_NAME_sparse_set_destroy(KL_NAME_SparseSet* set);
void kl_NAME_sparse_set_reset(KL_NAME_SparseSet* set);
T_X* kl_NAME_sparse_set_set(KL_NAME_SparseSet* set, int64_t id, T_X value);
T_X* kl_NAME_sparse_set_get(KL_NAME_SparseSet* set, int64_t id);
bool kl_NAME_sparse_set_has(KL_NAME_SparseSet* set, int64_t id);
bool kl_NAME_sparse_set_remove(KL_NAME_SparseSet* set, int64_t id);

#endif
