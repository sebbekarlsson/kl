/*
This file was automatically generated and must not be modified directly.
*/


#include <stdint.h>
#include <stdbool.h>
#include <kl/memory/allocator.h>
#include <kl/templates/x__shared.h>
#include <kl/templates/unique_list/shared.h>
#include <kl/templates/hashmap/int64_hashmap.h>
#include <kl/templates/list/list.h>
#include <kl/templates/unique_list/dummy.h>
#include <kl/binary/blob.h>
#include <kl/macros.h>
#include <string.h>
#include <assert.h>


#define KL_DEFINE_UNIQUE_LIST(NAME, T)                                                      \
  KL_DEFINE_LIST(NAME##__u, T);                                                             \
  typedef struct {                                                                          \
    KL_UniqueListConfig config;                                                             \
    KL_##NAME##__u_List _list;                                                              \
    KL_int64_Hashmap lookup;                                                                \
    bool initialized;                                                                       \
  } KL_##NAME##_UniqueList;                                                                 \
  int kl_##NAME##_unique_list_init(KL_##NAME##_UniqueList* list, KL_UniqueListConfig cfg);  \
  void kl_##NAME##_unique_list_destroy(KL_##NAME##_UniqueList* list);                       \
  void kl_##NAME##_unique_list_reset(KL_##NAME##_UniqueList* list);                         \
  bool kl_##NAME##_unique_list_push(KL_##NAME##_UniqueList *list, T item);                  \
  bool kl_##NAME##_unique_list_pop(KL_##NAME##_UniqueList* list, T* out);                   \
  bool kl_##NAME##_unique_list_pop_at(KL_##NAME##_UniqueList* list, int64_t idx, T* out);   \
  void kl_##NAME##_unique_list_grow(KL_##NAME##_UniqueList *list, int64_t n_slots);         \
  bool kl_##NAME##_unique_list_includes(KL_##NAME##_UniqueList* list, T item);              \
  int64_t kl_##NAME##_unique_list_index_of(KL_##NAME##_UniqueList* list, T item);



#define KL_IMPL_UNIQUE_LIST(NAME, T)                                                                              \
  int kl_##NAME##_unique_list_init(KL_##NAME##_UniqueList* list, KL_UniqueListConfig cfg) {                       \
    if(list->initialized) return 1;                                                                               \
    assert(cfg.capacity > 0);                                                                                     \
                                                                                                                  \
    if (cfg.collision_capacity <= 0) cfg.collision_capacity = 4;                                                  \
                                                                                                                  \
    list->config = cfg;                                                                                           \
                                                                                                                  \
    if (!kl_##NAME##__u_list_init(&list->_list, (KL_ListConfig){                                                  \
      .allocator = cfg.allocator,                                                                                 \
      .capacity = cfg.capacity                                                                                    \
    })) return 0;                                                                                                 \
    kl_int64_hashmap_init(&list->lookup, (KL_HashmapConfig){                                                      \
      .allocator = cfg.allocator,                                                                                 \
      .capacity = KL_MAX(cfg.capacity, 103),                                                                      \
      .track_used_slots = true,                                                                                   \
      .collision_capacity = cfg.collision_capacity                                                                \
    });                                                                                                           \
    list->initialized = true;                                                                                     \
    return 1;                                                                                                     \
  }                                                                                                               \
  void kl_##NAME##_unique_list_destroy(KL_##NAME##_UniqueList* list) {                                            \
    assert(list->initialized);                                                                                    \
    kl_##NAME##__u_list_destroy(&list->_list);                                                                    \
    kl_int64_hashmap_destroy(&list->lookup);                                                                      \
    memset(list, 0, sizeof(KL_##NAME##_UniqueList));                                                              \
  }                                                                                                               \
  void kl_##NAME##_unique_list_reset(KL_##NAME##_UniqueList* list) {                                              \
    assert(list->initialized);                                                                                    \
    kl_##NAME##__u_list_reset(&list->_list);                                                                      \
    kl_int64_hashmap_reset(&list->lookup);                                                                        \
  }                                                                                                               \
  bool kl_##NAME##_unique_list_push(KL_##NAME##_UniqueList *list, T item) {                                       \
    assert(list->initialized);                                                                                    \
    if (kl_##NAME##_unique_list_includes(list, item)) return false;                                               \
    int64_t idx = list->_list.length;                                                                             \
    kl_##NAME##__u_list_push(&list->_list, item);                                                                 \
    kl_int64_hashmap_set(&list->lookup, KL_BLOB_AUTO(item), idx);                                                 \
    return true;                                                                                                  \
  }                                                                                                               \
  bool kl_##NAME##_unique_list_pop(KL_##NAME##_UniqueList* list, T* out) {                                        \
    assert(list->initialized);                                                                                    \
    if (list->_list.length <= 0 || list->_list.items == 0) return false;                                          \
    return kl_##NAME##_unique_list_pop_at(list, list->_list.length-1, out);                                       \
  }                                                                                                               \
  bool kl_##NAME##_unique_list_pop_at(KL_##NAME##_UniqueList* list, int64_t idx, T* out) {                        \
    assert(list->initialized);                                                                                    \
    if (idx < 0 || idx >= list->_list.length || list->_list.length <= 0 || list->_list.items == 0) return false;  \
    T item = list->_list.items[idx];                                                                              \
    for (int64_t i = 0; i < list->lookup.used_slots.length; i++) {                                                \
      int64_t slot_idx = list->lookup.used_slots.items[i];                                                        \
      KL_int64_HashmapSlot* slot = &list->lookup.slots.items[slot_idx];                                           \
      if (slot->value > idx) {                                                                                    \
        slot->value -= 1;                                                                                         \
        assert(slot->value >= 0);                                                                                 \
      }                                                                                                           \
    }                                                                                                             \
                                                                                                                  \
    kl_int64_hashmap_unset(&list->lookup, KL_BLOB_AUTO(item));                                                    \
    return kl_##NAME##__u_list_pop_at(&list->_list, idx, out) == 1;                                               \
  }                                                                                                               \
  void kl_##NAME##_unique_list_grow(KL_##NAME##_UniqueList *list, int64_t n_slots) {                              \
    assert(list->initialized);                                                                                    \
    kl_##NAME##__u_list_grow(&list->_list, n_slots);                                                              \
  }                                                                                                               \
  bool kl_##NAME##_unique_list_includes(KL_##NAME##_UniqueList* list, T item) {                                   \
    assert(list->initialized);                                                                                    \
    return kl_int64_hashmap_has(&list->lookup, KL_BLOB_AUTO(item));                                               \
  }                                                                                                               \
  int64_t kl_##NAME##_unique_list_index_of(KL_##NAME##_UniqueList* list, T item) {                                \
    assert(list->initialized);                                                                                    \
    int64_t idx = -1;                                                                                             \
    if (!kl_int64_hashmap_get(&list->lookup, KL_BLOB_AUTO(item), &idx)) return -1;                                \
    assert(idx >= 0);                                                                                             \
    assert(idx < list->_list.length);                                                                             \
    return idx;                                                                                                   \
  }

/* generated */
