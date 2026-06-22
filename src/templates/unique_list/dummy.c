#include <kl/templates/unique_list/dummy.h>
#include <kl/binary/blob.h>
#include <kl/macros.h>
#include <string.h>
#include <assert.h>

int kl_NAME_unique_list_init(KL_NAME_UniqueList* list, KL_UniqueListConfig cfg) {
  if(list->initialized) return 1;
  assert(cfg.capacity > 0);
  
  if (cfg.collision_capacity <= 0) cfg.collision_capacity = 4;
  
  list->config = cfg;
  
  if (!kl_NAME__u_list_init(&list->_list, (KL_ListConfig){
    .allocator = cfg.allocator,
    .capacity = cfg.capacity
  })) return 0;

  kl_int64_hashmap_init(&list->lookup, (KL_HashmapConfig){
    .allocator = cfg.allocator,
    .capacity = KL_MAX(cfg.capacity, 103),
    .track_used_slots = true,
    .collision_capacity = cfg.collision_capacity 
  });

  list->initialized = true;
  return 1;
}

void kl_NAME_unique_list_destroy(KL_NAME_UniqueList* list) {
  assert(list->initialized);
  kl_NAME__u_list_destroy(&list->_list);
  kl_int64_hashmap_destroy(&list->lookup);
  memset(list, 0, sizeof(KL_NAME_UniqueList));
}

void kl_NAME_unique_list_reset(KL_NAME_UniqueList* list) {
  assert(list->initialized);
  kl_NAME__u_list_reset(&list->_list);
  kl_int64_hashmap_reset(&list->lookup);
}

bool kl_NAME_unique_list_push(KL_NAME_UniqueList *list, T_X item) {
  assert(list->initialized);
  if (kl_NAME_unique_list_includes(list, item)) return false;
  int64_t idx = list->_list.length;
  kl_NAME__u_list_push(&list->_list, item);
  kl_int64_hashmap_set(&list->lookup, KL_BLOB_AUTO(item), idx);
  return true;
}

bool kl_NAME_unique_list_pop(KL_NAME_UniqueList* list, T_X* out) {
  assert(list->initialized);
  if (list->_list.length <= 0 || list->_list.items == 0) return false;
  return kl_NAME_unique_list_pop_at(list, list->_list.length-1, out);
}

bool kl_NAME_unique_list_pop_at(KL_NAME_UniqueList* list, int64_t idx, T_X* out) {
  assert(list->initialized);
  if (idx < 0 || idx >= list->_list.length || list->_list.length <= 0 || list->_list.items == 0) return false;
  T_X item = list->_list.items[idx];

  // update indices
  for (int64_t i = 0; i < list->lookup.used_slots.length; i++) {
    int64_t slot_idx = list->lookup.used_slots.items[i];
    KL_int64_HashmapSlot* slot = &list->lookup.slots.items[slot_idx];

    if (slot->value > idx) {
      slot->value -= 1;
      assert(slot->value >= 0);
    }
  }
  
  kl_int64_hashmap_unset(&list->lookup, KL_BLOB_AUTO(item));
  return kl_NAME__u_list_pop_at(&list->_list, idx, out) == 1;
}

bool kl_NAME_unique_list_pop_unordered_at(KL_NAME_UniqueList* list, int64_t idx, T_X* out) {
  assert(list->initialized);
  if (idx < 0 || idx >= list->_list.length || list->_list.length <= 0 || list->_list.items == 0) return false;
  T_X item = list->_list.items[idx];

  // update indices
  for (int64_t i = 0; i < list->lookup.used_slots.length; i++) {
    int64_t slot_idx = list->lookup.used_slots.items[i];
    KL_int64_HashmapSlot* slot = &list->lookup.slots.items[slot_idx];

    if (slot->value > idx) {
      slot->value -= 1;
      assert(slot->value >= 0);
    }
  }
  
  kl_int64_hashmap_unset(&list->lookup, KL_BLOB_AUTO(item));
  return kl_NAME__u_list_pop_unordered_at(&list->_list, idx, out) == 1;
}

void kl_NAME_unique_list_grow(KL_NAME_UniqueList *list, int64_t n_slots) {
  assert(list->initialized);
  kl_NAME__u_list_grow(&list->_list, n_slots);
}

bool kl_NAME_unique_list_includes(KL_NAME_UniqueList* list, T_X item) {
  assert(list->initialized);
  return kl_int64_hashmap_has(&list->lookup, KL_BLOB_AUTO(item));
}

int64_t kl_NAME_unique_list_index_of(KL_NAME_UniqueList* list, T_X item) {
  assert(list->initialized);
  int64_t idx = -1;
  if (!kl_int64_hashmap_get(&list->lookup, KL_BLOB_AUTO(item), &idx)) return -1;
  assert(idx >= 0);
  assert(idx < list->_list.length);
  return idx;
}

bool kl_NAME_unique_list_remove(KL_NAME_UniqueList* list, T_X item) {
  assert(list->initialized);
  int64_t idx = kl_NAME_unique_list_index_of(list, item);
  if (idx < 0) return false;
  return kl_NAME_unique_list_pop_at(list, idx, 0);
}
