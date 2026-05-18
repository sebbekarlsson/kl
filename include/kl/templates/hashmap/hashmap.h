/*
This file was automatically generated and must not be modified directly.

Generated at: 2026-05-18 21:45:36
*/


#include <stdlib.h>
#include <kl/binary/blob.h>
#include <kl/memory/allocator.h>
#include <kl/templates/hashmap/shared.h>
#include <kl/templates/x__shared.h>
#include <stdbool.h>
#include <kl/templates/list/int64_list.h>
#include <assert.h>
#include <stdint.h>
#include <kl/templates/hashmap/dummy.h>
#include <kl/templates/list/blob_ptr_list.h>
#include <pthread.h>
#include <kl/templates/list/list.h>
#include <stdio.h>
#include <string.h>


#define KL_DEFINE_HASHMAP(NAME, T)                                                                       \
  typedef struct {                                                                                       \
    uint64_t hash;                                                                                       \
    int64_t next;                                                                                        \
    KL_Blob key;                                                                                         \
    T value;                                                                                             \
    bool is_set;                                                                                         \
  } KL_##NAME##_HashmapSlot;                                                                             \
  KL_DEFINE_LIST(NAME##_hashmap_slot, KL_##NAME##_HashmapSlot);                                          \
  typedef struct {                                                                                       \
    bool initialized;                                                                                    \
    KL_HashmapConfig config;                                                                             \
    /* capacity rounded up to a power of two; effective table size used for index masking */             \
    int64_t capacity_pow2;                                                                               \
    /* capacity_pow2 - 1, used to replace `% capacity` with `& capacity_mask` */                         \
    uint64_t capacity_mask;                                                                              \
    KL_Allocator key_allocator;                                                                          \
    KL_int64_List used_slots;                                                                            \
    struct {                                                                                             \
      KL_##NAME##_HashmapSlot* items;                                                                    \
      volatile int64_t length;                                                                           \
      volatile int64_t n_allocated;                                                                      \
    } slots;                                                                                             \
    int64_t n_collisions;                                                                                \
    pthread_mutex_t write_lock;                                                                          \
  } KL_##NAME##_Hashmap;                                                                                 \
  int kl_##NAME##_hashmap_init(KL_##NAME##_Hashmap* map, KL_HashmapConfig cfg);                          \
  void kl_##NAME##_hashmap_destroy(KL_##NAME##_Hashmap* map);                                            \
  int64_t kl_##NAME##_hashmap_set(KL_##NAME##_Hashmap* map, KL_Blob key, T value);                       \
  int kl_##NAME##_hashmap_get(KL_##NAME##_Hashmap* map, KL_Blob key, T* out);                            \
  int kl_##NAME##_hashmap_get_ptr(KL_##NAME##_Hashmap* map, KL_Blob key, T** out);                       \
  KL_HashmapHash kl_##NAME##_hashmap_get_hash(KL_##NAME##_Hashmap *map, KL_Blob key);                    \
  KL_##NAME##_HashmapSlot *kl_##NAME##_hashmap_get_slot(KL_##NAME##_Hashmap *map, KL_Blob key,           \
                                              KL_HashmapHash hash,                                       \
                                              int64_t slot_index);                                       \
  bool kl_##NAME##_hashmap_has(KL_##NAME##_Hashmap* map, KL_Blob key);                                   \
  int kl_##NAME##_hashmap_unset(KL_##NAME##_Hashmap* map, KL_Blob key);                                  \
  int kl_##NAME##_hashmap_get_keys(KL_##NAME##_Hashmap* map, KL_blob_ptr_List* out);                     \
  int kl_##NAME##_hashmap_get_used_slots(KL_##NAME##_Hashmap* map, KL_##NAME##_hashmap_slot_List* out);  \
  void kl_##NAME##_hashmap_serialize_to_file(KL_##NAME##_Hashmap map, FILE* fp);                         \
  int kl_##NAME##_hashmap_deserialize_from_file(KL_##NAME##_Hashmap* map, FILE* fp);                     \
  void kl_##NAME##_hashmap_reset(KL_##NAME##_Hashmap* map);                                              \
  void kl_##NAME##_hashmap_print(KL_##NAME##_Hashmap* map, FILE* fp);



#define KL_IMPL_HASHMAP(NAME, T)                                                                                             \
  KL_IMPL_LIST(NAME##_hashmap_slot, KL_##NAME##_HashmapSlot);                                                                \
  static KL_##NAME##_HashmapSlot *kl_##NAME##_hashmap_new_slot(KL_##NAME##_Hashmap *map) {                                   \
    int64_t avail = map->slots.n_allocated - map->slots.length;                                                              \
    if (avail <= 0) {                                                                                                        \
      int64_t grow = KL_MAX((int64_t)1, (map->capacity_pow2 / 3) + (map->n_collisions / 2));                                 \
      KL_##NAME##_HashmapSlot *tmp = (KL_##NAME##_HashmapSlot *)realloc(                                                     \
          map->slots.items,                                                                                                  \
          (map->slots.n_allocated + grow) * sizeof(KL_##NAME##_HashmapSlot));                                                \
      if (tmp == 0) return 0;                                                                                                \
      map->slots.items = tmp;                                                                                                \
      map->slots.n_allocated += grow;                                                                                        \
    }                                                                                                                        \
    assert(map->slots.items != 0);                                                                                           \
    KL_##NAME##_HashmapSlot *slot = &map->slots.items[map->slots.length];                                                    \
    memset(slot, 0, sizeof(*slot));                                                                                          \
    slot->next = -1;                                                                                                         \
    map->slots.length += 1;                                                                                                  \
    return slot;                                                                                                             \
  }                                                                                                                          \
  int kl_##NAME##_hashmap_init(KL_##NAME##_Hashmap *map, KL_HashmapConfig cfg) {                                             \
    assert(cfg.capacity > 0);                                                                                                \
    assert(cfg.collision_capacity > 0);                                                                                      \
    assert(cfg.max_key_size > 0);                                                                                            \
    if (map->initialized)                                                                                                    \
      return 1;                                                                                                              \
    /* Round capacity up to the next power of two so `hash % capacity` becomes                                               \
       `hash & (capacity-1)` on the hot path. */                                                                             \
    int64_t pow2 = 1;                                                                                                        \
    while (pow2 < cfg.capacity) pow2 <<= 1;                                                                                  \
    map->config = cfg;                                                                                                       \
    map->capacity_pow2 = pow2;                                                                                               \
    map->capacity_mask = (uint64_t)(pow2 - 1);                                                                               \
    map->slots.length = 0;                                                                                                   \
    map->slots.items =                                                                                                       \
        (KL_##NAME##_HashmapSlot *)calloc(pow2, sizeof(KL_##NAME##_HashmapSlot));                                            \
    assert(map->slots.items != 0);                                                                                           \
    map->slots.n_allocated = pow2;                                                                                           \
    kl_allocator_init(                                                                                                       \
        &map->key_allocator,                                                                                                 \
        (KL_AllocatorConfig){.capacity = (pow2 + cfg.collision_capacity) * cfg.max_key_size});                               \
    if (cfg.track_used_slots) {                                                                                              \
      kl_int64_list_init(&map->used_slots, (KL_ListConfig){                                                                  \
        .capacity = pow2                                                                                                     \
      });                                                                                                                    \
    }                                                                                                                        \
    /* Initialize the home slots: zero already from calloc, but `next` must be                                               \
       -1 (0 is a valid slot index, so memset alone is wrong). */                                                            \
    for (int64_t i = 0; i < pow2; i++) {                                                                                     \
      map->slots.items[i].next = -1;                                                                                         \
    }                                                                                                                        \
    map->slots.length = pow2;                                                                                                \
    map->n_collisions = 0;                                                                                                   \
    if (pthread_mutex_init(&map->write_lock, 0) != 0) {                                                                      \
      fprintf(stderr, "Failed to initialize mutex.\n");                                                                      \
      if (map->slots.items != 0) {                                                                                           \
        free(map->slots.items);                                                                                              \
        map->slots.items = 0;                                                                                                \
      }                                                                                                                      \
      map->slots.length = 0;                                                                                                 \
      map->slots.n_allocated = 0;                                                                                            \
      kl_allocator_destroy(&map->key_allocator);                                                                             \
      if (map->used_slots.initialized) {                                                                                     \
        kl_int64_list_destroy(&map->used_slots);                                                                             \
      }                                                                                                                      \
      map->config = (KL_HashmapConfig){0};                                                                                   \
      return 0;                                                                                                              \
    }                                                                                                                        \
    map->initialized = true;                                                                                                 \
    return 1;                                                                                                                \
  }                                                                                                                          \
  void kl_##NAME##_hashmap_destroy(KL_##NAME##_Hashmap *map) {                                                               \
    assert(map->initialized == true);                                                                                        \
    if (map->slots.items != 0) {                                                                                             \
      free(map->slots.items);                                                                                                \
    }                                                                                                                        \
    map->slots.items = 0;                                                                                                    \
    map->slots.length = 0;                                                                                                   \
    map->n_collisions = 0;                                                                                                   \
    map->slots.n_allocated = 0;                                                                                              \
    kl_allocator_destroy(&map->key_allocator);                                                                               \
    if (map->used_slots.initialized) {                                                                                       \
      kl_int64_list_destroy(&map->used_slots);                                                                               \
    }                                                                                                                        \
    map->config = (KL_HashmapConfig){0};                                                                                     \
    pthread_mutex_destroy(&map->write_lock);                                                                                 \
    map->initialized = false;                                                                                                \
  }                                                                                                                          \
  static inline uint64_t kl_##NAME##_hashmap_murmur64_scramble(uint64_t k) {                                                 \
    k ^= k >> 33ULL;                                                                                                         \
    k *= 0xff51afd7ed558ccdULL;                                                                                              \
    k ^= k >> 33ULL;                                                                                                         \
    k *= 0xc4ceb9fe1a85ec53ULL;                                                                                              \
    k ^= k >> 33ULL;                                                                                                         \
    return k;                                                                                                                \
  }                                                                                                                          \
  KL_HashmapHash kl_##NAME##_hashmap_get_hash(KL_##NAME##_Hashmap *map, KL_Blob key) {                                       \
    KL_HashmapHash hash = {0};                                                                                               \
    uint64_t mask = map->capacity_mask;                                                                                      \
    if (key.size <= (int64_t)sizeof(int64_t) && map->config.max_key_size <= (int64_t)sizeof(int64_t)) {                      \
      uint64_t h = 0;                                                                                                        \
      memcpy(&h, key.value, key.size);                                                                                       \
      h = kl_##NAME##_hashmap_murmur64_scramble(h | (uint64_t)key.size << 56);                                               \
      hash.hash = h;                                                                                                         \
      hash.index = (int64_t)(h & mask);                                                                                      \
      return hash;                                                                                                           \
    }                                                                                                                        \
    uint64_t h = 14695981039346656037ULL;                                                                                    \
    for (uint64_t i = 0; i < (uint64_t)key.size; i++) {                                                                      \
      h ^= (uint8_t)key.value[i];                                                                                            \
      h *= 1099511628211ULL;                                                                                                 \
    }                                                                                                                        \
    h = kl_##NAME##_hashmap_murmur64_scramble(h);                                                                            \
    hash.hash = h;                                                                                                           \
    hash.index = (int64_t)(h & mask);                                                                                        \
    return hash;                                                                                                             \
  }                                                                                                                          \
  static inline bool kl_##NAME##_hashmap_compare(KL_##NAME##_HashmapSlot *slot, KL_Blob key, uint64_t hash_value) {          \
    /* Callers gate on slot->is_set, which guarantees slot->key.value != 0. */                                               \
    if (slot->hash != hash_value || slot->key.size != key.size) {                                                            \
      return false;                                                                                                          \
    }                                                                                                                        \
    return memcmp(slot->key.value, key.value, (size_t)key.size) == 0;                                                        \
  }                                                                                                                          \
  static void kl_##NAME##_hashmap_assign_key(KL_##NAME##_Hashmap *map, KL_##NAME##_HashmapSlot *slot, KL_Blob key) {         \
    assert(key.size <= map->config.max_key_size);                                                                            \
    if (slot->key.value == 0) {                                                                                              \
      slot->key.value = kl_allocator_malloc(&map->key_allocator, map->config.max_key_size);                                  \
      assert(slot->key.value != 0);                                                                                          \
    }                                                                                                                        \
    assert(slot->key.value != 0);                                                                                            \
    slot->key.size = key.size;                                                                                               \
    memcpy(slot->key.value, key.value, key.size);                                                                            \
  }                                                                                                                          \
  KL_##NAME##_HashmapSlot *kl_##NAME##_hashmap_get_slot(KL_##NAME##_Hashmap *map, KL_Blob key,                               \
                                              KL_HashmapHash hash,                                                           \
                                              int64_t slot_index) {                                                          \
    if (slot_index < 0 || slot_index >= map->slots.length) return 0;                                                         \
    /* Walk the intrusive single-link chain from the home slot. */                                                           \
    KL_##NAME##_HashmapSlot *slots = map->slots.items;                                                                       \
    uint64_t hv = hash.hash;                                                                                                 \
    int64_t cur = slot_index;                                                                                                \
    do {                                                                                                                     \
      KL_##NAME##_HashmapSlot *s = &slots[cur];                                                                              \
      if (s->is_set && kl_##NAME##_hashmap_compare(s, key, hv)) {                                                            \
        return s;                                                                                                            \
      }                                                                                                                      \
      cur = s->next;                                                                                                         \
    } while (cur >= 0);                                                                                                      \
    return 0;                                                                                                                \
  }                                                                                                                          \
  static int64_t kl_##NAME##_hashmap_set_slot(KL_##NAME##_Hashmap *map, KL_Blob key,                                         \
                                         KL_HashmapHash hash, T value) {                                                     \
    int64_t home_idx = hash.index;                                                                                           \
    KL_##NAME##_HashmapSlot *slots = map->slots.items;                                                                       \
    uint64_t hv = hash.hash;                                                                                                 \
    /* Single-pass walk: find first matching key, else first reusable empty slot. */                                         \
    int64_t match_idx = -1;                                                                                                  \
    int64_t empty_idx = -1;                                                                                                  \
    int64_t cur = home_idx;                                                                                                  \
    do {                                                                                                                     \
      KL_##NAME##_HashmapSlot *s = &slots[cur];                                                                              \
      if (s->is_set) {                                                                                                       \
        if (kl_##NAME##_hashmap_compare(s, key, hv)) {                                                                       \
          match_idx = cur;                                                                                                   \
          break;                                                                                                             \
        }                                                                                                                    \
      } else if (empty_idx < 0) {                                                                                            \
        empty_idx = cur;                                                                                                     \
      }                                                                                                                      \
      cur = s->next;                                                                                                         \
    } while (cur >= 0);                                                                                                      \
    int64_t slot_idx;                                                                                                        \
    bool was_set;                                                                                                            \
    if (match_idx >= 0) {                                                                                                    \
      slot_idx = match_idx;                                                                                                  \
      was_set = true;                                                                                                        \
    } else if (empty_idx >= 0) {                                                                                             \
      slot_idx = empty_idx;                                                                                                  \
      was_set = false;                                                                                                       \
    } else {                                                                                                                 \
      KL_##NAME##_HashmapSlot *fresh = kl_##NAME##_hashmap_new_slot(map);                                                    \
      if (fresh == 0) return -1;                                                                                             \
      map->n_collisions += 1;                                                                                                \
      /* new_slot may realloc slots.items; recompute pointers. */                                                            \
      slots = map->slots.items;                                                                                              \
      slot_idx = (int64_t)(fresh - slots);                                                                                   \
      /* Push-front into home's chain. */                                                                                    \
      slots[slot_idx].next = slots[home_idx].next;                                                                           \
      slots[home_idx].next = slot_idx;                                                                                       \
      was_set = false;                                                                                                       \
    }                                                                                                                        \
    KL_##NAME##_HashmapSlot *slot = &slots[slot_idx];                                                                        \
    kl_##NAME##_hashmap_assign_key(map, slot, key);                                                                          \
    slot->value = value;                                                                                                     \
    if (map->used_slots.initialized && !was_set) {                                                                           \
      kl_int64_list_push(&map->used_slots, slot_idx);                                                                        \
    }                                                                                                                        \
    slot->is_set = true;                                                                                                     \
    slot->hash = hv;                                                                                                         \
    return slot_idx;                                                                                                         \
  }                                                                                                                          \
  int64_t kl_##NAME##_hashmap_set(KL_##NAME##_Hashmap *map, KL_Blob key, T value) {                                          \
    assert(map->initialized == true);                                                                                        \
    assert(key.value != 0);                                                                                                  \
    assert(key.size > 0);                                                                                                    \
    KL_HashmapHash hash = kl_##NAME##_hashmap_get_hash(map, key);                                                            \
    int64_t result = -1;                                                                                                     \
    pthread_mutex_lock(&map->write_lock);                                                                                    \
    result = kl_##NAME##_hashmap_set_slot(map, key, hash, value);                                                            \
    pthread_mutex_unlock(&map->write_lock);                                                                                  \
    return result;                                                                                                           \
  }                                                                                                                          \
  int kl_##NAME##_hashmap_get(KL_##NAME##_Hashmap* map, KL_Blob key, T* out) {                                               \
    assert(map->initialized == true);                                                                                        \
    assert(key.value != 0);                                                                                                  \
    assert(key.size > 0);                                                                                                    \
    assert(out != 0);                                                                                                        \
    KL_HashmapHash hash = kl_##NAME##_hashmap_get_hash(map, key);                                                            \
    KL_##NAME##_HashmapSlot *slot = kl_##NAME##_hashmap_get_slot(map, key, hash, hash.index);                                \
    if (slot != 0 && slot->is_set) {                                                                                         \
      *out = slot->value;                                                                                                    \
      return 1;                                                                                                              \
    }                                                                                                                        \
    return 0;                                                                                                                \
  }                                                                                                                          \
  int kl_##NAME##_hashmap_get_ptr(KL_##NAME##_Hashmap* map, KL_Blob key, T** out) {                                          \
    assert(map->initialized == true);                                                                                        \
    assert(key.value != 0);                                                                                                  \
    assert(key.size > 0);                                                                                                    \
    assert(out != 0);                                                                                                        \
    KL_HashmapHash hash = kl_##NAME##_hashmap_get_hash(map, key);                                                            \
    KL_##NAME##_HashmapSlot *slot = kl_##NAME##_hashmap_get_slot(map, key, hash, hash.index);                                \
    if (slot != 0 && slot->is_set) {                                                                                         \
      *out = &slot->value;                                                                                                   \
      return 1;                                                                                                              \
    }                                                                                                                        \
    return 0;                                                                                                                \
  }                                                                                                                          \
  bool kl_##NAME##_hashmap_has(KL_##NAME##_Hashmap *map, KL_Blob key) {                                                      \
    assert(map->initialized == true);                                                                                        \
    assert(key.value != 0);                                                                                                  \
    assert(key.size > 0);                                                                                                    \
    KL_HashmapHash hash = kl_##NAME##_hashmap_get_hash(map, key);                                                            \
    KL_##NAME##_HashmapSlot *slot = kl_##NAME##_hashmap_get_slot(map, key, hash, hash.index);                                \
    return slot != 0 && slot->is_set;                                                                                        \
  }                                                                                                                          \
  int kl_##NAME##_hashmap_unset(KL_##NAME##_Hashmap* map, KL_Blob key) {                                                     \
    assert(map->initialized == true);                                                                                        \
    assert(key.value != 0);                                                                                                  \
    assert(key.size > 0);                                                                                                    \
    pthread_mutex_lock(&map->write_lock);                                                                                    \
    KL_HashmapHash hash = kl_##NAME##_hashmap_get_hash(map, key);                                                            \
    KL_##NAME##_HashmapSlot *slot = kl_##NAME##_hashmap_get_slot(map, key, hash, hash.index);                                \
    if (slot == 0 || slot->is_set == false) {                                                                                \
      pthread_mutex_unlock(&map->write_lock);                                                                                \
      return 0;                                                                                                              \
    }                                                                                                                        \
    int64_t slot_id = (int64_t)(slot - map->slots.items);                                                                    \
    if (map->used_slots.initialized) {                                                                                       \
      for (int64_t i = map->used_slots.length-1; i >= 0; i--) {                                                              \
        if (map->used_slots.items[i] == slot_id) {                                                                           \
          kl_int64_list_pop_at(&map->used_slots, i, 0);                                                                      \
          break;                                                                                                             \
        }                                                                                                                    \
      }                                                                                                                      \
    }                                                                                                                        \
    slot->is_set = false;                                                                                                    \
    slot->hash = 0;                                                                                                          \
    pthread_mutex_unlock(&map->write_lock);                                                                                  \
    return 1;                                                                                                                \
  }                                                                                                                          \
  int kl_##NAME##_hashmap_get_keys(KL_##NAME##_Hashmap* map, KL_blob_ptr_List* out) {                                        \
    assert(map->initialized == true);                                                                                        \
    assert(out != 0);                                                                                                        \
    if (!out->initialized) {                                                                                                 \
      kl_blob_ptr_list_init(out, (KL_ListConfig){ .capacity = map->config.capacity });                                       \
    }                                                                                                                        \
    for (int64_t i = 0; i < map->slots.length; i++) {                                                                        \
      KL_##NAME##_HashmapSlot* slot = &map->slots.items[i];                                                                  \
      if (!slot->is_set) continue;                                                                                           \
      assert(slot->key.value != 0);                                                                                          \
      assert(slot->key.size > 0);                                                                                            \
      kl_blob_ptr_list_push(out, &slot->key);                                                                                \
    }                                                                                                                        \
    return out->length > 0 && out->items != 0;                                                                               \
  }                                                                                                                          \
  int kl_##NAME##_hashmap_get_used_slots(KL_##NAME##_Hashmap* map, KL_##NAME##_hashmap_slot_List* out) {                     \
    assert(map->initialized == true);                                                                                        \
    assert(out != 0);                                                                                                        \
    if (!out->initialized) {                                                                                                 \
      kl_##NAME##_hashmap_slot_list_init(out, (KL_ListConfig){ .capacity = map->config.capacity });                          \
    }                                                                                                                        \
    for (int64_t i = 0; i < map->slots.length; i++) {                                                                        \
      KL_##NAME##_HashmapSlot* slot = &map->slots.items[i];                                                                  \
      if (!slot->is_set) continue;                                                                                           \
      assert(slot->key.value != 0);                                                                                          \
      assert(slot->key.size > 0);                                                                                            \
      kl_##NAME##_hashmap_slot_list_push(out, *slot);                                                                        \
    }                                                                                                                        \
    return out->length > 0 && out->items != 0;                                                                               \
  }                                                                                                                          \
  static void kl_##NAME##_hashmap_serialize_slot(KL_##NAME##_HashmapSlot slot, FILE* fp) {                                   \
    /* Wire layout (v2):                                                                                                     \
         uint64_t hash                                                                                                       \
         int64_t  next                                                                                                       \
         uint8_t  is_set                                                                                                     \
         int64_t  key.size                                                                                                   \
         [bytes]  key.value (key.size bytes; only if size > 0)                                                               \
         T      value                                                                                                        \
    */                                                                                                                       \
    fwrite(&slot.hash, sizeof(uint64_t), 1, fp);                                                                             \
    fwrite(&slot.next, sizeof(int64_t), 1, fp);                                                                              \
    uint8_t is_set_byte = slot.is_set ? 1 : 0;                                                                               \
    fwrite(&is_set_byte, sizeof(uint8_t), 1, fp);                                                                            \
    fwrite(&slot.key.size, sizeof(int64_t), 1, fp);                                                                          \
    if (slot.key.value != 0 && slot.key.size > 0) {                                                                          \
      fwrite(slot.key.value, (size_t)slot.key.size, 1, fp);                                                                  \
    }                                                                                                                        \
    fwrite(&slot.value, sizeof(T), 1, fp);                                                                                   \
  }                                                                                                                          \
  static void kl_##NAME##_hashmap_deserialize_slot(KL_##NAME##_Hashmap* map, KL_##NAME##_HashmapSlot* slot, FILE* fp) {      \
    fread(&slot->hash, sizeof(uint64_t), 1, fp);                                                                             \
    fread(&slot->next, sizeof(int64_t), 1, fp);                                                                              \
    uint8_t is_set_byte = 0;                                                                                                 \
    fread(&is_set_byte, sizeof(uint8_t), 1, fp);                                                                             \
    slot->is_set = is_set_byte != 0;                                                                                         \
    fread(&slot->key.size, sizeof(int64_t), 1, fp);                                                                          \
    if (slot->key.size > 0) {                                                                                                \
      slot->key.value = kl_allocator_malloc(&map->key_allocator, map->config.max_key_size);                                  \
      assert(slot->key.value != 0);                                                                                          \
      fread(slot->key.value, (size_t)slot->key.size, 1, fp);                                                                 \
    } else {                                                                                                                 \
      slot->key.value = 0;                                                                                                   \
    }                                                                                                                        \
    fread(&slot->value, sizeof(T), 1, fp);                                                                                   \
  }                                                                                                                          \
  void kl_##NAME##_hashmap_serialize_to_file(KL_##NAME##_Hashmap map, FILE* fp) {                                            \
    assert(map.initialized == true);                                                                                         \
    assert(fp != 0);                                                                                                         \
    uint32_t magic = KL_HASHMAP_MAGIC__SHARED;                                                                               \
    fwrite(&magic, sizeof(uint32_t), 1, fp);                                                                                 \
    fwrite(&map.config, sizeof(KL_HashmapConfig), 1, fp);                                                                    \
    int64_t len = map.slots.length;                                                                                          \
    fwrite(&len, sizeof(int64_t), 1, fp);                                                                                    \
    if (map.slots.items != 0 && map.slots.length > 0) {                                                                      \
      for (int64_t i = 0; i < map.slots.length; i++) {                                                                       \
        kl_##NAME##_hashmap_serialize_slot(map.slots.items[i], fp);                                                          \
      }                                                                                                                      \
    }                                                                                                                        \
  }                                                                                                                          \
  int kl_##NAME##_hashmap_deserialize_from_file(KL_##NAME##_Hashmap* map, FILE* fp) {                                        \
    assert(map != 0);                                                                                                        \
    assert(map->initialized == false);                                                                                       \
    assert(fp != 0);                                                                                                         \
    uint32_t magic = 0;                                                                                                      \
    fread(&magic, sizeof(uint32_t), 1, fp);                                                                                  \
    if (magic != KL_HASHMAP_MAGIC__SHARED) {                                                                                 \
      fprintf(stderr, "Invalid magic: 0x%08X (expected 0x%08X). Old format files are no longer supported.\n",                \
              magic, KL_HASHMAP_MAGIC__SHARED);                                                                              \
      return 0;                                                                                                              \
    }                                                                                                                        \
    fread(&map->config, sizeof(KL_HashmapConfig), 1, fp);                                                                    \
    kl_##NAME##_hashmap_init(map, map->config);                                                                              \
    int64_t slots_length = 0;                                                                                                \
    fread(&slots_length, sizeof(int64_t), 1, fp);                                                                            \
    if (slots_length > 0) {                                                                                                  \
      for (int64_t i = 0; i < slots_length; i++) {                                                                           \
        KL_##NAME##_HashmapSlot* slot = i >= map->slots.length ?  kl_##NAME##_hashmap_new_slot(map) : &map->slots.items[i];  \
        assert(slot != 0);                                                                                                   \
        kl_##NAME##_hashmap_deserialize_slot(map, slot, fp);                                                                 \
      }                                                                                                                      \
    }                                                                                                                        \
    return 1;                                                                                                                \
  }                                                                                                                          \
  void kl_##NAME##_hashmap_reset(KL_##NAME##_Hashmap* map) {                                                                 \
    assert(map != 0);                                                                                                        \
    assert(map->initialized == true);                                                                                        \
    KL_HashmapConfig cfg = map->config;                                                                                      \
    kl_##NAME##_hashmap_destroy(map);                                                                                        \
    kl_##NAME##_hashmap_init(map, cfg);                                                                                      \
  }                                                                                                                          \
  static void kl_##NAME##_hashmap_print_slot(KL_##NAME##_Hashmap* map, KL_##NAME##_HashmapSlot* slot, FILE* fp, int pad) {   \
    int64_t id = (int64_t)(slot - map->slots.items);                                                                         \
    fprintf(fp, "%*sSlot {\n", pad, "");                                                                                     \
    fprintf(fp, "%*sid: %ld\n", 2+pad, "", id);                                                                              \
    fprintf(fp, "%*shash: 0x%016lX\n", 2+pad, "", slot->hash);                                                               \
    if (slot->next >= 0) {                                                                                                   \
      fprintf(fp, "%*snext: [\n", 2+pad, "");                                                                                \
      int64_t cur = slot->next;                                                                                              \
      while (cur >= 0) {                                                                                                     \
        assert(cur >= 0);                                                                                                    \
        assert(cur < map->slots.length);                                                                                     \
        KL_##NAME##_HashmapSlot* other = &map->slots.items[cur];                                                             \
        kl_##NAME##_hashmap_print_slot(map, other, fp, pad+2+2);                                                             \
        cur = other->next;                                                                                                   \
      }                                                                                                                      \
      fprintf(fp, "%*s]\n", 2+pad, "");                                                                                      \
    }                                                                                                                        \
    fprintf(fp, "%*s}\n", pad, "");                                                                                          \
  }                                                                                                                          \
  void kl_##NAME##_hashmap_print(KL_##NAME##_Hashmap* map, FILE* fp) {                                                       \
    assert(map != 0);                                                                                                        \
    assert(map->initialized == true);                                                                                        \
    assert(fp != 0);                                                                                                         \
    fprintf(fp, "HashMap {\n");                                                                                              \
    for (int64_t i = 0; i < map->slots.length; i++) {                                                                        \
      KL_##NAME##_HashmapSlot* slot = &map->slots.items[i];                                                                  \
      if (!(slot->is_set || slot->next >= 0)) continue;                                                                      \
      kl_##NAME##_hashmap_print_slot(map, slot, fp, 2);                                                                      \
    }                                                                                                                        \
    fprintf(fp, "}\n");                                                                                                      \
  }

/* generated */
