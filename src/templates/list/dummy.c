#include <assert.h>
#include <kl/memory/allocator.h>
#include <kl/memory/alloc.h>
#include <kl/templates/list/dummy.h>
#include <kl/macros.h>
#include <stdlib.h>
#include <string.h>
#include <stdalign.h>

int kl_NAME_list_init(KL_NAME_List *list, KL_ListConfig cfg) {
  assert(cfg.capacity > 0);
  if (list->initialized)
    return 1;

  list->alignment = alignof(T_X);
  list->config = cfg;
  list->items = 0;
  list->length = 0;
  list->n_slots_allocated = 0;
  list->current_capacity = cfg.capacity;
  list->allocation = (KL_Allocation){0};

  list->initialized = true;
  return 1;
}

void kl_NAME_list_destroy(KL_NAME_List *list) {
  if (list->config.allocator == 0) {
    if (list->items != 0) {
      free(list->items);
    }
  }
  
  list->items = 0;
  list->config = (KL_ListConfig){0};
  list->length = 0;
  list->n_slots_allocated = 0;
  list->current_capacity = 0;
  list->initialized = false;
}

void kl_NAME_list_reset(KL_NAME_List *list) {
  if (list->items != 0) {
    if (list->n_slots_allocated > 0) {
      memset(&list->items[0], 0, list->n_slots_allocated * sizeof(T_X));
    }
  }
  list->length = 0;
}

void kl_NAME_list_grow(KL_NAME_List *list, int64_t n_slots) {
  if (list->config.allocator != 0) {
    list->items = (T_X *)kl_allocator_realloc_aligned(
        list->config.allocator,
        (list->n_slots_allocated + n_slots) * sizeof(T_X), list->alignment, &list->allocation);
    assert(list->items != 0);
    memset(&list->items[list->n_slots_allocated], 0, n_slots * sizeof(T_X));
    list->n_slots_allocated += n_slots;
    return;
  }

  list->items = (T_X *)kl_aligned_realloc(
    list->items,
    (list->n_slots_allocated + n_slots) * sizeof(T_X),
    list->alignment
  );
  assert(list->items != 0);
  memset(&list->items[list->n_slots_allocated], 0, n_slots * sizeof(T_X));
  list->n_slots_allocated += n_slots;
}

void kl_NAME_list_concat(KL_NAME_List *list, const KL_NAME_List *other) {
  assert(list->initialized == true);

  if (other == 0 || other->items == 0 || other->length <= 0)
    return;

  int64_t avail = list->n_slots_allocated - list->length;
  if (avail < other->length) {
    kl_NAME_list_grow(list, other->length - avail);
  }
  assert(list->items != 0);

  memcpy(&list->items[list->length], &other->items[0],
         other->length * sizeof(T_X));
  list->length += other->length;
}

void kl_NAME_list_push(KL_NAME_List *list, T_X item) {
  assert(list->initialized == true);
  assert(list->current_capacity > 0);

  int64_t avail = list->n_slots_allocated - list->length;
  if (avail <= 0) {
    kl_NAME_list_grow(list, list->current_capacity);
    list->current_capacity += KL_MAX(list->current_capacity / 3, 1);
  }
  assert(list->items != 0);

  list->items[list->length++] = item;
}

void kl_NAME_list_push_start(KL_NAME_List *list, T_X item) {
  assert(list->initialized == true);
  assert(list->current_capacity > 0);

  int64_t avail = list->n_slots_allocated - list->length;
  if (avail <= 0) {
    kl_NAME_list_grow(list, list->current_capacity);
    list->current_capacity += KL_MAX(list->current_capacity / 3, 1);
  }
  assert(list->items != 0);

  for (int64_t i = list->length; i > 0; i--) {
    list->items[i] = list->items[i - 1];
  }

  list->items[0] = item;
  list->length++;
}

int kl_NAME_list_pop(KL_NAME_List *list, T_X *out) {
  assert(list->initialized == true);

  if (list->length <= 0 || list->items == 0)
    return 0;

  if (out != 0) {
    *out = list->items[list->length - 1];
  }

  list->length -= 1;

  return 1;
}

int kl_NAME_list_pop_at(KL_NAME_List *list, int64_t idx, T_X *out) {
  assert(list->initialized == true);

  if (list->length <= 0 || list->items == 0)
    return 0;

  assert(idx < list->length);
  assert(idx >= 0);

  if (out != 0) {
    *out = list->items[idx];
  }

  for (int64_t i = idx; i < list->length - 1; i++) {
    list->items[i] = list->items[i + 1];
  }

  list->length -= 1;

  return 1;
}

int kl_NAME_list_pop_unordered_at(KL_NAME_List* list, int64_t idx, T_X* out) {
  assert(list->initialized == true);

  if (list->length <= 0 || list->items == 0)
    return 0;

  assert(idx < list->length);
  assert(idx >= 0);

  if (out != 0) {
    *out = list->items[idx];
  }

  list->items[idx] = list->items[list->length-1];

  list->length -= 1;

  return 1;
}

bool kl_NAME_list_includes(KL_NAME_List *list, T_X item) {
  if (list == 0 || list->items == 0 || list->length <= 0)
    return false;

  for (int64_t i = 0; i < list->length; i++) {
    if (memcmp(&list->items[i], &item, sizeof(T_X)) == 0)
      return true;
  }

  return false;
}

int64_t kl_NAME_list_index_of(KL_NAME_List* list, T_X item) {
  if (list == 0) return -1;
  
  assert(list->initialized);
  
  if (list->length <= 0 || list->items == 0) return -1;

  for (int64_t i = 0; i < list->length; i++) {
    T_X v = list->items[i];
    if (memcmp(&v, &item, sizeof(T_X)) == 0) return i;
  }

  return -1;
}
