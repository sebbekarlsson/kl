#include <assert.h>
#include <kl/memory/allocator.h>
#include <kl/templates/queue/dummy.h>
#include <kl/macros.h>
#include <stdlib.h>
#include <string.h>

bool kl_NAME_queue_init(KL_NAME_Queue *queue, KL_QueueConfig cfg) {
  assert(cfg.capacity > 0);
  if (queue->initialized)
    return true;

  queue->config = cfg;
  queue->items = 0;
  queue->length = 0;
  queue->n_slots_allocated = 0;
  queue->current_capacity = cfg.capacity;
  queue->allocation = (KL_Allocation){0};
  kl_mutex_init(&queue->lock);

  queue->initialized = true;
  return true;
}

void kl_NAME_queue_destroy(KL_NAME_Queue *queue) {
  assert(queue->initialized == true);
  
  kl_mutex_lock(&queue->lock);
  
  if (queue->config.allocator == 0) {
    if (queue->items != 0) {
      free(queue->items);
    }
  }
  queue->items = 0;
  queue->config = (KL_QueueConfig){0};
  queue->length = 0;
  queue->n_slots_allocated = 0;
  queue->current_capacity = 0;
  queue->initialized = false;
  
  kl_mutex_unlock(&queue->lock);
  
  kl_mutex_destroy(&queue->lock);
}

void kl_NAME_queue_reset(KL_NAME_Queue *queue) {
  kl_mutex_lock(&queue->lock);
  
  if (queue->items != 0) {
    if (queue->n_slots_allocated > 0) {
      memset(&queue->items[0], 0, queue->n_slots_allocated * sizeof(T_X));
    }
  }
  queue->length = 0;

  kl_mutex_unlock(&queue->lock);
}

void kl_NAME_queue_grow(KL_NAME_Queue *queue, int64_t n_slots) {
  if (queue->config.allocator != 0) {
    queue->items = (T_X *)kl_allocator_realloc(
        queue->config.allocator,
        (queue->n_slots_allocated + n_slots) * sizeof(T_X), &queue->allocation);
    assert(queue->items != 0);
    memset(&queue->items[queue->n_slots_allocated], 0, n_slots * sizeof(T_X));
    queue->n_slots_allocated += n_slots;
    return;
  }

  queue->items = (T_X *)realloc(
      queue->items, (queue->n_slots_allocated + n_slots) * sizeof(T_X));
  assert(queue->items != 0);
  memset(&queue->items[queue->n_slots_allocated], 0, n_slots * sizeof(T_X));
  queue->n_slots_allocated += n_slots;
}

static inline bool kl_NAME_queue_push_(KL_NAME_Queue *queue, T_X item) {
  assert(queue->initialized == true);
  assert(queue->current_capacity > 0);
  

  if (queue->config.max_length > 0 && queue->length >= queue->config.max_length) {
    return false;
  }

  int64_t avail = queue->n_slots_allocated - queue->length;
  if (avail <= 0) {
    kl_NAME_queue_grow(queue, queue->current_capacity);
    queue->current_capacity += KL_MAX(queue->current_capacity / 3, 1);
  }
  assert(queue->items != 0);

  queue->items[queue->length++] = item;


  return true;
}

bool kl_NAME_queue_push(KL_NAME_Queue *queue, T_X item) {
  kl_mutex_lock(&queue->lock);
  bool ret = kl_NAME_queue_push_(queue, item);
  kl_mutex_unlock(&queue->lock);
  return ret;
}


bool kl_NAME_queue_try_push(KL_NAME_Queue* queue, T_X item) {
  if (!kl_mutex_trylock(&queue->lock)) return false;
  bool ret = kl_NAME_queue_push_(queue, item);
  kl_mutex_unlock(&queue->lock);
  return ret;
}


static inline bool kl_NAME_queue_pop_at_(KL_NAME_Queue *queue, int64_t idx, T_X *out) {
  assert(queue->initialized == true);

  if (queue->length <= 0 || queue->items == 0 || idx < 0) {
    return false;
  }

  assert(idx < queue->length);
  assert(idx >= 0);

  if (out != 0) {
    *out = queue->items[idx];
  }

  for (int64_t i = idx; i < queue->length - 1; i++) {
    queue->items[i] = queue->items[i + 1];
  }

  queue->length -= 1;


  return true;
}

bool kl_NAME_queue_pop_first(KL_NAME_Queue* queue, T_X* out) {
  kl_mutex_lock(&queue->lock);
  bool ret = kl_NAME_queue_pop_at_(queue, 0, out);
  kl_mutex_unlock(&queue->lock);
  return ret;
}

bool kl_NAME_queue_pop_last(KL_NAME_Queue* queue, T_X* out) {
  kl_mutex_lock(&queue->lock);
  bool ret = kl_NAME_queue_pop_at_(queue, queue->length - 1, out);
  kl_mutex_unlock(&queue->lock);
  return ret;
}

bool kl_NAME_queue_pop_at(KL_NAME_Queue *queue, int64_t idx, T_X *out) {
  kl_mutex_lock(&queue->lock);
  bool ret = kl_NAME_queue_pop_at_(queue, idx, out);
  kl_mutex_unlock(&queue->lock);
  return ret;
}

bool kl_NAME_queue_try_pop_first(KL_NAME_Queue* queue, T_X* out) {
  if (!kl_mutex_trylock(&queue->lock)) return false;
  bool ret = kl_NAME_queue_pop_at_(queue, 0, out);
  kl_mutex_unlock(&queue->lock);
  return ret;
}

bool kl_NAME_queue_try_pop_last(KL_NAME_Queue* queue, T_X* out) {
  if (!kl_mutex_trylock(&queue->lock)) return false;
  bool ret = kl_NAME_queue_pop_at_(queue, queue->length - 1, out);
  kl_mutex_unlock(&queue->lock);
  return ret;
}

bool kl_NAME_queue_try_pop_at(KL_NAME_Queue* queue, int64_t idx, T_X* out) {
  if (!kl_mutex_trylock(&queue->lock)) return false;
  bool ret = kl_NAME_queue_pop_at_(queue, idx, out);
  kl_mutex_unlock(&queue->lock);
  return ret;
}
