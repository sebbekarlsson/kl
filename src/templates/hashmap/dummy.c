#include <kl/memory/allocator.h>
#include <kl/binary/blob.h>
#include <kl/templates/list/int64_list.h>
#include <assert.h>
#include <kl/templates/hashmap/dummy.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

KL_IMPL_LIST(NAME_hashmap_slot, KL_NAME_HashmapSlot);

static KL_NAME_HashmapSlot *kl_NAME_hashmap_new_slot(KL_NAME_Hashmap *map) {
  int64_t avail = map->slots.n_allocated - map->slots.length;
  if (avail <= 0) {
    int64_t grow = KL_MAX((int64_t)1, (map->capacity_pow2 / 3) + (map->n_collisions / 2));
    KL_NAME_HashmapSlot *tmp = 0;

    if (map->config.allocator != 0) {
      tmp = (KL_NAME_HashmapSlot *)kl_allocator_realloc_aligned(map->config.allocator,
      (map->slots.n_allocated + grow) * sizeof(KL_NAME_HashmapSlot), alignof(KL_NAME_HashmapSlot), &map->slots.allocation);
      assert(tmp != 0);
    } else {
      tmp = (KL_NAME_HashmapSlot *)realloc(
        map->slots.items,
        (map->slots.n_allocated + grow) * sizeof(KL_NAME_HashmapSlot));
    }
    if (tmp == 0) return 0;
    map->slots.items = tmp;
    map->slots.n_allocated += grow;
  }
  assert(map->slots.items != 0);
  KL_NAME_HashmapSlot *slot = &map->slots.items[map->slots.length];
  memset(slot, 0, sizeof(*slot));
  slot->next = -1;
  map->slots.length += 1;
  return slot;
}

int kl_NAME_hashmap_init(KL_NAME_Hashmap *map, KL_HashmapConfig cfg) {
  assert(cfg.capacity > 0);
  assert(cfg.collision_capacity > 0);
  if (map->initialized)
    return 1;

  /* Round capacity up to the next power of two so `hash % capacity` becomes
     `hash & (capacity-1)` on the hot path. */
  int64_t pow2 = 1;
  while (pow2 < cfg.capacity) pow2 <<= 1;

  map->config = cfg;
  map->capacity_pow2 = pow2;
  map->capacity_mask = (uint64_t)(pow2 - 1);
  map->slots.length = 0;

  if (cfg.allocator != 0) {
    map->slots.items = (KL_NAME_HashmapSlot*)kl_allocator_malloc_tracked_aligned(cfg.allocator, pow2*sizeof(KL_NAME_HashmapSlot), alignof(KL_NAME_HashmapSlot), &map->slots.allocation);
    assert(map->slots.items != 0);
  } else {
    map->slots.items =
      (KL_NAME_HashmapSlot *)calloc(pow2, sizeof(KL_NAME_HashmapSlot));
  }
  assert(map->slots.items != 0);
  map->slots.n_allocated = pow2;

  /* Per-slot key buffers are carved from this arena and grow on demand; the
     capacity here is only an initial page-size hint (~32 bytes per key). */
  kl_allocator_init(
      &map->key_allocator,
      (KL_AllocatorConfig){.capacity = (pow2 + cfg.collision_capacity) * 32});

  if (cfg.track_used_slots) {
    kl_int64_list_init(&map->used_slots, (KL_ListConfig){
      .capacity = pow2,
      .allocator = map->config.allocator
    });
  }

  /* Initialize the home slots: zero already from calloc, but `next` must be
     -1 (0 is a valid slot index, so memset alone is wrong). */
  for (int64_t i = 0; i < pow2; i++) {
    map->slots.items[i].next = -1;
  }
  map->slots.length = pow2;

  map->n_collisions = 0;

  if (pthread_mutex_init(&map->write_lock, 0) != 0) {
    fprintf(stderr, "Failed to initialize mutex.\n");
    if (map->slots.items != 0 && map->config.allocator == 0) {
      free(map->slots.items);
      map->slots.items = 0;
    }
    map->slots.length = 0;
    map->slots.n_allocated = 0;
    kl_allocator_destroy(&map->key_allocator);
    if (map->used_slots.initialized) {
      kl_int64_list_destroy(&map->used_slots);
    }
    map->config = (KL_HashmapConfig){0};
    return 0;
  }

  map->initialized = true;
  return 1;
}

void kl_NAME_hashmap_destroy(KL_NAME_Hashmap *map) {
  assert(map->initialized == true);

  if (map->slots.items != 0 && map->config.allocator == 0) {
    free(map->slots.items);
  }
  map->slots.items = 0;
  map->slots.length = 0;
  map->n_collisions = 0;
  map->slots.n_allocated = 0;
  kl_allocator_destroy(&map->key_allocator);

  if (map->used_slots.initialized) {
    kl_int64_list_destroy(&map->used_slots);
  }

  map->config = (KL_HashmapConfig){0};

  pthread_mutex_destroy(&map->write_lock);

  map->initialized = false;
}

static inline uint64_t kl_NAME_hashmap_murmur64_scramble(uint64_t k) {
  k ^= k >> 33ULL;
  k *= 0xff51afd7ed558ccdULL;
  k ^= k >> 33ULL;
  k *= 0xc4ceb9fe1a85ec53ULL;
  k ^= k >> 33ULL;
  return k;
}


KL_HashmapHash kl_NAME_hashmap_get_hash(KL_NAME_Hashmap *map, KL_Blob key) {
  KL_HashmapHash hash = {0};
  uint64_t mask = map->capacity_mask;

  if (key.size <= (int64_t)sizeof(int64_t)) {
    uint64_t h = 0;
    memcpy(&h, key.value, key.size);
    h = kl_NAME_hashmap_murmur64_scramble(h | (uint64_t)key.size << 56);
    hash.hash = h;
    hash.index = (int64_t)(h & mask);
    return hash;
  }

  uint64_t h = 14695981039346656037ULL;
  for (uint64_t i = 0; i < (uint64_t)key.size; i++) {
    h ^= (uint8_t)key.value[i];
    h *= 1099511628211ULL;
  }

  h = kl_NAME_hashmap_murmur64_scramble(h);
  hash.hash = h;
  hash.index = (int64_t)(h & mask);

  return hash;
}

static inline bool kl_NAME_hashmap_compare(KL_NAME_HashmapSlot *slot, KL_Blob key, uint64_t hash_value) {
  /* Callers gate on slot->is_set, which guarantees slot->key.value != 0. */
  if (slot->hash != hash_value || slot->key.size != key.size) {
    return false;
  }
  return memcmp(slot->key.value, key.value, (size_t)key.size) == 0;
}

/* A slot keeps its key buffer across re-use and only grows it when a longer
   key arrives; the old, smaller buffer is left behind in the arena and
   reclaimed wholesale on destroy/reset. */
static void kl_NAME_hashmap_assign_key(KL_NAME_Hashmap *map, KL_NAME_HashmapSlot *slot, KL_Blob key) {
  if (key.size > slot->key_capacity) {
    slot->key.value = kl_allocator_malloc(map->config.allocator ? map->config.allocator : &map->key_allocator, key.size);
    assert(slot->key.value != 0);
    slot->key_capacity = key.size;
  }
  assert(slot->key.value != 0);
  slot->key.size = key.size;
  memcpy(slot->key.value, key.value, key.size);
}

KL_NAME_HashmapSlot *kl_NAME_hashmap_get_slot(KL_NAME_Hashmap *map, KL_Blob key,
                                            KL_HashmapHash hash,
                                            int64_t slot_index) {
  if (slot_index < 0 || slot_index >= map->slots.length) return 0;

  /* Walk the intrusive single-link chain from the home slot. */
  KL_NAME_HashmapSlot *slots = map->slots.items;
  uint64_t hv = hash.hash;
  int64_t cur = slot_index;
  do {
    KL_NAME_HashmapSlot *s = &slots[cur];
    if (s->is_set && kl_NAME_hashmap_compare(s, key, hv)) {
      return s;
    }
    cur = s->next;
  } while (cur >= 0);
  return 0;
}

static int64_t kl_NAME_hashmap_set_slot(KL_NAME_Hashmap *map, KL_Blob key,
                                       KL_HashmapHash hash, T_X value) {
  int64_t home_idx = hash.index;
  KL_NAME_HashmapSlot *slots = map->slots.items;
  uint64_t hv = hash.hash;

  /* Single-pass walk: find first matching key, else first reusable empty slot. */
  int64_t match_idx = -1;
  int64_t empty_idx = -1;
  int64_t cur = home_idx;
  do {
    KL_NAME_HashmapSlot *s = &slots[cur];
    if (s->is_set) {
      if (kl_NAME_hashmap_compare(s, key, hv)) {
        match_idx = cur;
        break;
      }
    } else if (empty_idx < 0) {
      empty_idx = cur;
    }
    cur = s->next;
  } while (cur >= 0);

  int64_t slot_idx;
  bool was_set;
  if (match_idx >= 0) {
    slot_idx = match_idx;
    was_set = true;
  } else if (empty_idx >= 0) {
    slot_idx = empty_idx;
    was_set = false;
  } else {
    KL_NAME_HashmapSlot *fresh = kl_NAME_hashmap_new_slot(map);
    if (fresh == 0) return -1;
    map->n_collisions += 1;
    /* new_slot may realloc slots.items; recompute pointers. */
    slots = map->slots.items;
    slot_idx = (int64_t)(fresh - slots);
    /* Push-front into home's chain. */
    slots[slot_idx].next = slots[home_idx].next;
    slots[home_idx].next = slot_idx;
    was_set = false;
  }

  KL_NAME_HashmapSlot *slot = &slots[slot_idx];
  kl_NAME_hashmap_assign_key(map, slot, key);
  slot->value = value;

  if (map->used_slots.initialized && !was_set) {
    kl_int64_list_push(&map->used_slots, slot_idx);
  }

  slot->is_set = true;
  slot->hash = hv;
  return slot_idx;
}

int64_t kl_NAME_hashmap_set(KL_NAME_Hashmap *map, KL_Blob key, T_X value) {
  assert(map->initialized == true);
  assert(key.value != 0);
  assert(key.size > 0);
  KL_HashmapHash hash = kl_NAME_hashmap_get_hash(map, key);

  int64_t result = -1;
  pthread_mutex_lock(&map->write_lock);
  result = kl_NAME_hashmap_set_slot(map, key, hash, value);
  pthread_mutex_unlock(&map->write_lock);
  return result;
}

int kl_NAME_hashmap_get(KL_NAME_Hashmap* map, KL_Blob key, T_X* out) {
  assert(map->initialized == true);
  assert(key.value != 0);
  assert(key.size > 0);
  assert(out != 0);
  KL_HashmapHash hash = kl_NAME_hashmap_get_hash(map, key);
  KL_NAME_HashmapSlot *slot = kl_NAME_hashmap_get_slot(map, key, hash, hash.index);
  if (slot != 0 && slot->is_set) {
    *out = slot->value;
    return 1;
  }
  return 0;
}

int kl_NAME_hashmap_get_ptr(KL_NAME_Hashmap* map, KL_Blob key, T_X** out) {
  assert(map->initialized == true);
  assert(key.value != 0);
  assert(key.size > 0);
  assert(out != 0);
  KL_HashmapHash hash = kl_NAME_hashmap_get_hash(map, key);
  KL_NAME_HashmapSlot *slot = kl_NAME_hashmap_get_slot(map, key, hash, hash.index);
  if (slot != 0 && slot->is_set) {
    *out = &slot->value;
    return 1;
  }
  return 0;
}

bool kl_NAME_hashmap_has(KL_NAME_Hashmap *map, KL_Blob key) {
  assert(map->initialized == true);
  assert(key.value != 0);
  assert(key.size > 0);
  KL_HashmapHash hash = kl_NAME_hashmap_get_hash(map, key);
  KL_NAME_HashmapSlot *slot = kl_NAME_hashmap_get_slot(map, key, hash, hash.index);
  return slot != 0 && slot->is_set;
}

int kl_NAME_hashmap_unset(KL_NAME_Hashmap* map, KL_Blob key) {
  assert(map->initialized == true);
  assert(key.value != 0);
  assert(key.size > 0);

  pthread_mutex_lock(&map->write_lock);

  KL_HashmapHash hash = kl_NAME_hashmap_get_hash(map, key);
  KL_NAME_HashmapSlot *slot = kl_NAME_hashmap_get_slot(map, key, hash, hash.index);
  if (slot == 0 || slot->is_set == false) {
    pthread_mutex_unlock(&map->write_lock);
    return 0;
  }

  int64_t slot_id = (int64_t)(slot - map->slots.items);

  if (map->used_slots.initialized) {
    for (int64_t i = map->used_slots.length-1; i >= 0; i--) {
      if (map->used_slots.items[i] == slot_id) {
        kl_int64_list_pop_at(&map->used_slots, i, 0);
        break;
      }
    }
  }

  slot->is_set = false;
  slot->hash = 0;
  pthread_mutex_unlock(&map->write_lock);

  return 1;
}

int kl_NAME_hashmap_get_keys(KL_NAME_Hashmap* map, KL_blob_ptr_List* out) {
  assert(map->initialized == true);
  assert(out != 0);

  if (!out->initialized) {
    kl_blob_ptr_list_init(out, (KL_ListConfig){ .capacity = map->config.capacity });
  }

  for (int64_t i = 0; i < map->slots.length; i++) {
    KL_NAME_HashmapSlot* slot = &map->slots.items[i];
    if (!slot->is_set) continue;
    assert(slot->key.value != 0);
    assert(slot->key.size > 0);
    kl_blob_ptr_list_push(out, &slot->key);
  }

  return out->length > 0 && out->items != 0;
}

int kl_NAME_hashmap_get_used_slots(KL_NAME_Hashmap* map, KL_NAME_hashmap_slot_List* out) {
  assert(map->initialized == true);
  assert(out != 0);

  if (!out->initialized) {
    kl_NAME_hashmap_slot_list_init(out, (KL_ListConfig){ .capacity = map->config.capacity });
  }

  for (int64_t i = 0; i < map->slots.length; i++) {
    KL_NAME_HashmapSlot* slot = &map->slots.items[i];
    if (!slot->is_set) continue;
    assert(slot->key.value != 0);
    assert(slot->key.size > 0);
    kl_NAME_hashmap_slot_list_push(out, *slot);
  }

  return out->length > 0 && out->items != 0;
}

static void kl_NAME_hashmap_serialize_slot(KL_NAME_HashmapSlot slot, FILE* fp) {
  /* Wire layout (v2):
       uint64_t hash
       int64_t  next
       uint8_t  is_set
       int64_t  key.size
       [bytes]  key.value (key.size bytes; only if size > 0)
       T_X      value
  */
  fwrite(&slot.hash, sizeof(uint64_t), 1, fp);
  fwrite(&slot.next, sizeof(int64_t), 1, fp);
  uint8_t is_set_byte = slot.is_set ? 1 : 0;
  fwrite(&is_set_byte, sizeof(uint8_t), 1, fp);
  fwrite(&slot.key.size, sizeof(int64_t), 1, fp);
  if (slot.key.value != 0 && slot.key.size > 0) {
    fwrite(slot.key.value, (size_t)slot.key.size, 1, fp);
  }
  fwrite(&slot.value, sizeof(T_X), 1, fp);
}

static void kl_NAME_hashmap_deserialize_slot(KL_NAME_Hashmap* map, KL_NAME_HashmapSlot* slot, FILE* fp) {
  fread(&slot->hash, sizeof(uint64_t), 1, fp);
  fread(&slot->next, sizeof(int64_t), 1, fp);
  uint8_t is_set_byte = 0;
  fread(&is_set_byte, sizeof(uint8_t), 1, fp);
  slot->is_set = is_set_byte != 0;
  fread(&slot->key.size, sizeof(int64_t), 1, fp);
  if (slot->key.size > 0) {
    slot->key.value = kl_allocator_malloc(map->config.allocator ? map->config.allocator : &map->key_allocator, slot->key.size);
    assert(slot->key.value != 0);
    slot->key_capacity = slot->key.size;
    fread(slot->key.value, (size_t)slot->key.size, 1, fp);
  } else {
    slot->key.value = 0;
    slot->key_capacity = 0;
  }
  fread(&slot->value, sizeof(T_X), 1, fp);
}

void kl_NAME_hashmap_serialize_to_file(KL_NAME_Hashmap map, FILE* fp) {
  assert(map.initialized == true);
  assert(fp != 0);

  uint32_t magic = KL_HASHMAP_MAGIC__SHARED;
  fwrite(&magic, sizeof(uint32_t), 1, fp);

  /* Route the config through a zeroed struct before writing: an fwrite of the
     raw struct would otherwise emit its uninitialised trailing padding (the
     bytes after `track_used_slots`) into the file. */
  KL_HashmapConfig config_out;
  memset(&config_out, 0, sizeof(config_out));
  config_out.capacity = map.config.capacity;
  config_out.collision_capacity = map.config.collision_capacity;
  config_out.track_used_slots = map.config.track_used_slots;
  fwrite(&config_out, sizeof(KL_HashmapConfig), 1, fp);
  int64_t len = map.slots.length;
  fwrite(&len, sizeof(int64_t), 1, fp);
  if (map.slots.items != 0 && map.slots.length > 0) {
    for (int64_t i = 0; i < map.slots.length; i++) {
      kl_NAME_hashmap_serialize_slot(map.slots.items[i], fp);
    }
  }
}

int kl_NAME_hashmap_deserialize_from_file(KL_NAME_Hashmap* map, FILE* fp) {
  assert(map != 0);
  assert(map->initialized == false);
  assert(fp != 0);

  uint32_t magic = 0;
  fread(&magic, sizeof(uint32_t), 1, fp);

  if (magic != KL_HASHMAP_MAGIC__SHARED) {
    return 0;
  }

  fread(&map->config, sizeof(KL_HashmapConfig), 1, fp);
  kl_NAME_hashmap_init(map, map->config);

  int64_t slots_length = 0;
  fread(&slots_length, sizeof(int64_t), 1, fp);
  if (slots_length > 0) {
    for (int64_t i = 0; i < slots_length; i++) {
      KL_NAME_HashmapSlot* slot = i >= map->slots.length ?  kl_NAME_hashmap_new_slot(map) : &map->slots.items[i];
      assert(slot != 0);
      kl_NAME_hashmap_deserialize_slot(map, slot, fp);
    }
  }

  return 1;
}

void kl_NAME_hashmap_reset(KL_NAME_Hashmap* map) {
  assert(map != 0);
  assert(map->initialized == true);
  KL_HashmapConfig cfg = map->config;
  kl_NAME_hashmap_destroy(map);
  kl_NAME_hashmap_init(map, cfg);
}

static void kl_NAME_hashmap_print_slot(KL_NAME_Hashmap* map, KL_NAME_HashmapSlot* slot, FILE* fp, int pad) {
  int64_t id = (int64_t)(slot - map->slots.items);
  fprintf(fp, "%*sSlot {\n", pad, "");
  fprintf(fp, "%*sid: %ld\n", 2+pad, "", id);
  fprintf(fp, "%*shash: 0x%016lX\n", 2+pad, "", slot->hash);
  if (slot->next >= 0) {
    fprintf(fp, "%*snext: [\n", 2+pad, "");
    int64_t cur = slot->next;
    while (cur >= 0) {
      assert(cur >= 0);
      assert(cur < map->slots.length);
      KL_NAME_HashmapSlot* other = &map->slots.items[cur];
      kl_NAME_hashmap_print_slot(map, other, fp, pad+2+2);
      cur = other->next;
    }
    fprintf(fp, "%*s]\n", 2+pad, "");
  }
  fprintf(fp, "%*s}\n", pad, "");
}

void kl_NAME_hashmap_print(KL_NAME_Hashmap* map, FILE* fp) {
  assert(map != 0);
  assert(map->initialized == true);
  assert(fp != 0);

  fprintf(fp, "HashMap {\n");
  for (int64_t i = 0; i < map->slots.length; i++) {
    KL_NAME_HashmapSlot* slot = &map->slots.items[i];
    if (!(slot->is_set || slot->next >= 0)) continue;
    kl_NAME_hashmap_print_slot(map, slot, fp, 2);
  }
  fprintf(fp, "}\n");
}
