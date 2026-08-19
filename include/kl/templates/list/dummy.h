#ifndef KL_LIST_NAME_H
#define KL_LIST_NAME_H
#include <stdint.h>
#include <stdint.h>
#include <stdbool.h>
#include <kl/memory/allocator.h>
#include <kl/templates/x__shared.h>
#include <kl/templates/list/shared.h>

typedef struct {
  KL_ListConfig config;
  size_t alignment;
  T_X* items;
  KL_Allocation allocation;
  volatile int64_t n_slots_allocated;
  volatile int64_t length;
  volatile int64_t current_capacity;
  bool initialized;
} KL_NAME_List;

int kl_NAME_list_init(KL_NAME_List* list, KL_ListConfig cfg);

void kl_NAME_list_destroy(KL_NAME_List* list);

void kl_NAME_list_reset(KL_NAME_List* list);

void kl_NAME_list_reset_fast(KL_NAME_List* list);

void kl_NAME_list_push(KL_NAME_List *list, T_X item);

void kl_NAME_list_push_start(KL_NAME_List *list, T_X item);

int kl_NAME_list_pop(KL_NAME_List* list, T_X* out);

int kl_NAME_list_pop_at(KL_NAME_List* list, int64_t idx, T_X* out);

int kl_NAME_list_pop_unordered_at(KL_NAME_List* list, int64_t idx, T_X* out);

bool kl_NAME_list_remove(KL_NAME_List* list, T_X item);

bool kl_NAME_list_remove_unordered(KL_NAME_List* list, T_X item);

void kl_NAME_list_grow(KL_NAME_List *list, int64_t n_slots);

void kl_NAME_list_concat(KL_NAME_List *list, const KL_NAME_List *other);

void kl_NAME_list_splice(KL_NAME_List* list, int64_t index, int64_t to_remove, T_X item);

bool kl_NAME_list_includes(KL_NAME_List* list, T_X item);

int64_t kl_NAME_list_index_of(KL_NAME_List* list, T_X item);

#endif
