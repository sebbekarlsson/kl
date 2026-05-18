/*
This file was automatically generated and must not be modified directly.

Generated at: 2026-05-19 00:51:26
*/


#include <stdlib.h>
#include <stdbool.h>
#include <kl/macros.h>
#include <kl/templates/list/shared.h>
#include <stdalign.h>
#include <kl/memory/alloc.h>
#include <kl/templates/list/dummy.h>
#include <kl/templates/x__shared.h>
#include <stdint.h>
#include <string.h>
#include <kl/memory/allocator.h>
#include <assert.h>


#define KL_DEFINE_LIST(NAME, T)                                              \
  typedef struct {                                                           \
    KL_ListConfig config;                                                    \
    size_t alignment;                                                        \
    T* items;                                                                \
    KL_Allocation allocation;                                                \
    volatile int64_t n_slots_allocated;                                      \
    volatile int64_t length;                                                 \
    volatile int64_t current_capacity;                                       \
    bool initialized;                                                        \
  } KL_##NAME##_List;                                                        \
  int kl_##NAME##_list_init(KL_##NAME##_List* list, KL_ListConfig cfg);      \
  void kl_##NAME##_list_destroy(KL_##NAME##_List* list);                     \
  void kl_##NAME##_list_reset(KL_##NAME##_List* list);                       \
  void kl_##NAME##_list_push(KL_##NAME##_List *list, T item);                \
  void kl_##NAME##_list_push_start(KL_##NAME##_List *list, T item);          \
  int kl_##NAME##_list_pop(KL_##NAME##_List* list, T* out);                  \
  int kl_##NAME##_list_pop_at(KL_##NAME##_List* list, int64_t idx, T* out);  \
  void kl_##NAME##_list_grow(KL_##NAME##_List *list, int64_t n_slots);       \
  bool kl_##NAME##_list_includes(KL_##NAME##_List* list, T item);



#define KL_IMPL_LIST(NAME, T)                                                                    \
  int kl_##NAME##_list_init(KL_##NAME##_List *list, KL_ListConfig cfg) {                         \
    assert(cfg.capacity > 0);                                                                    \
    if (list->initialized)                                                                       \
      return 1;                                                                                  \
    list->alignment = alignof(T);                                                                \
    list->config = cfg;                                                                          \
    list->items = 0;                                                                             \
    list->length = 0;                                                                            \
    list->n_slots_allocated = 0;                                                                 \
    list->current_capacity = cfg.capacity;                                                       \
    list->allocation = (KL_Allocation){0};                                                       \
    list->initialized = true;                                                                    \
    return 1;                                                                                    \
  }                                                                                              \
  void kl_##NAME##_list_destroy(KL_##NAME##_List *list) {                                        \
    if (list->config.allocator == 0) {                                                           \
      if (list->items != 0) {                                                                    \
        free(list->items);                                                                       \
      }                                                                                          \
    }                                                                                            \
                                                                                                 \
    list->items = 0;                                                                             \
    list->config = (KL_ListConfig){0};                                                           \
    list->length = 0;                                                                            \
    list->n_slots_allocated = 0;                                                                 \
    list->current_capacity = 0;                                                                  \
    list->initialized = false;                                                                   \
  }                                                                                              \
  void kl_##NAME##_list_reset(KL_##NAME##_List *list) {                                          \
    if (list->items != 0) {                                                                      \
      if (list->n_slots_allocated > 0) {                                                         \
        memset(&list->items[0], 0, list->n_slots_allocated * sizeof(T));                         \
      }                                                                                          \
    }                                                                                            \
    list->length = 0;                                                                            \
  }                                                                                              \
  void kl_##NAME##_list_grow(KL_##NAME##_List *list, int64_t n_slots) {                          \
    if (list->config.allocator != 0) {                                                           \
      list->items = (T *)kl_allocator_realloc_aligned(                                           \
          list->config.allocator,                                                                \
          (list->n_slots_allocated + n_slots) * sizeof(T), list->alignment, &list->allocation);  \
      assert(list->items != 0);                                                                  \
      memset(&list->items[list->n_slots_allocated], 0, n_slots * sizeof(T));                     \
      list->n_slots_allocated += n_slots;                                                        \
      return;                                                                                    \
    }                                                                                            \
    list->items = (T *)kl_aligned_realloc(                                                       \
      list->items,                                                                               \
      (list->n_slots_allocated + n_slots) * sizeof(T),                                           \
      list->alignment                                                                            \
    );                                                                                           \
    assert(list->items != 0);                                                                    \
    memset(&list->items[list->n_slots_allocated], 0, n_slots * sizeof(T));                       \
    list->n_slots_allocated += n_slots;                                                          \
  }                                                                                              \
  void kl_##NAME##_list_push(KL_##NAME##_List *list, T item) {                                   \
    assert(list->initialized == true);                                                           \
    assert(list->current_capacity > 0);                                                          \
    int64_t avail = list->n_slots_allocated - list->length;                                      \
    if (avail <= 0) {                                                                            \
      kl_##NAME##_list_grow(list, list->current_capacity);                                       \
      list->current_capacity += KL_MAX(list->current_capacity / 3, 1);                           \
    }                                                                                            \
    assert(list->items != 0);                                                                    \
    list->items[list->length++] = item;                                                          \
  }                                                                                              \
  void kl_##NAME##_list_push_start(KL_##NAME##_List *list, T item) {                             \
    assert(list->initialized == true);                                                           \
    assert(list->current_capacity > 0);                                                          \
    int64_t avail = list->n_slots_allocated - list->length;                                      \
    if (avail <= 0) {                                                                            \
      kl_##NAME##_list_grow(list, list->current_capacity);                                       \
      list->current_capacity += KL_MAX(list->current_capacity / 3, 1);                           \
    }                                                                                            \
    assert(list->items != 0);                                                                    \
    for (int64_t i = list->length; i > 0; i--) {                                                 \
      list->items[i] = list->items[i - 1];                                                       \
    }                                                                                            \
    list->items[0] = item;                                                                       \
    list->length++;                                                                              \
  }                                                                                              \
  int kl_##NAME##_list_pop(KL_##NAME##_List *list, T *out) {                                     \
    assert(list->initialized == true);                                                           \
    if (list->length <= 0 || list->items == 0)                                                   \
      return 0;                                                                                  \
    if (out != 0) {                                                                              \
      *out = list->items[list->length - 1];                                                      \
    }                                                                                            \
    list->length -= 1;                                                                           \
    return 1;                                                                                    \
  }                                                                                              \
  int kl_##NAME##_list_pop_at(KL_##NAME##_List *list, int64_t idx, T *out) {                     \
    assert(list->initialized == true);                                                           \
    if (list->length <= 0 || list->items == 0)                                                   \
      return 0;                                                                                  \
    assert(idx < list->length);                                                                  \
    assert(idx >= 0);                                                                            \
    if (out != 0) {                                                                              \
      *out = list->items[idx];                                                                   \
    }                                                                                            \
    for (int64_t i = idx; i < list->length - 1; i++) {                                           \
      list->items[i] = list->items[i + 1];                                                       \
    }                                                                                            \
    list->length -= 1;                                                                           \
    return 1;                                                                                    \
  }                                                                                              \
  bool kl_##NAME##_list_includes(KL_##NAME##_List *list, T item) {                               \
    if (list == 0 || list->items == 0 || list->length <= 0)                                      \
      return false;                                                                              \
    for (int64_t i = 0; i < list->length; i++) {                                                 \
      if (memcmp(&list->items[i], &item, sizeof(T)) == 0)                                        \
        return true;                                                                             \
    }                                                                                            \
    return false;                                                                                \
  }

/* generated */
