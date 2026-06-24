#ifndef KL_HASHMAP_H
#define KL_HASHMAP_H
#include <kl/memory/allocator.h>
#include <kl/binary/blob.h>
#include <kl/templates/list/int64_list.h>
#include <kl/templates/list/blob_ptr_list.h>
#include <kl/templates/list/list.h>
#include <kl/templates/x__shared.h>
#include <kl/templates/hashmap/shared.h>
#include <stdbool.h>
#include <stdint.h>
#include <pthread.h>
#include <stdio.h>

typedef struct {
  uint64_t hash;
  int64_t next;
  KL_Blob key;
  /* bytes allocated for key.value in the arena; >= key.size, grows on demand */
  int64_t key_capacity;
  T_X value;
  bool is_set;
} KL_NAME_HashmapSlot;

KL_DEFINE_LIST(NAME_hashmap_slot, KL_NAME_HashmapSlot);

typedef struct {
  bool initialized;
  KL_HashmapConfig config;
  /* capacity rounded up to a power of two; effective table size used for index masking */
  int64_t capacity_pow2;
  /* capacity_pow2 - 1, used to replace `% capacity` with `& capacity_mask` */
  uint64_t capacity_mask;
  KL_Allocator key_allocator;
  KL_int64_List used_slots;

  struct {
    KL_NAME_HashmapSlot* items;
    volatile int64_t length;
    volatile int64_t n_allocated;
    KL_Allocation allocation;
  } slots;

  int64_t n_collisions;

  pthread_mutex_t write_lock;
} KL_NAME_Hashmap;

int kl_NAME_hashmap_init(KL_NAME_Hashmap* map, KL_HashmapConfig cfg);

void kl_NAME_hashmap_destroy(KL_NAME_Hashmap* map);

int64_t kl_NAME_hashmap_set(KL_NAME_Hashmap* map, KL_Blob key, T_X value);

int kl_NAME_hashmap_get(KL_NAME_Hashmap* map, KL_Blob key, T_X* out);

int kl_NAME_hashmap_get_ptr(KL_NAME_Hashmap* map, KL_Blob key, T_X** out);

KL_HashmapHash kl_NAME_hashmap_get_hash(KL_NAME_Hashmap *map, KL_Blob key);

KL_NAME_HashmapSlot *kl_NAME_hashmap_get_slot(KL_NAME_Hashmap *map, KL_Blob key,
                                            KL_HashmapHash hash,
                                            int64_t slot_index);

bool kl_NAME_hashmap_has(KL_NAME_Hashmap* map, KL_Blob key);

int kl_NAME_hashmap_unset(KL_NAME_Hashmap* map, KL_Blob key);

int kl_NAME_hashmap_get_keys(KL_NAME_Hashmap* map, KL_blob_ptr_List* out);

int kl_NAME_hashmap_get_used_slots(KL_NAME_Hashmap* map, KL_NAME_hashmap_slot_List* out);

void kl_NAME_hashmap_serialize_to_file(KL_NAME_Hashmap map, FILE* fp);

int kl_NAME_hashmap_deserialize_from_file(KL_NAME_Hashmap* map, FILE* fp);

void kl_NAME_hashmap_reset(KL_NAME_Hashmap* map);

void kl_NAME_hashmap_print(KL_NAME_Hashmap* map, FILE* fp);

void kl_NAME_hashmap_print_json(KL_NAME_Hashmap* map, FILE* fp);

#endif
