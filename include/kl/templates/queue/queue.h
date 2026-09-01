/*
This file was automatically generated and must not be modified directly.
*/


#include <stdint.h>
#include <stdbool.h>
#include <kl/memory/allocator.h>
#include <kl/threads/mutex.h>
#include <kl/templates/x__shared.h>
#include <kl/templates/queue/shared.h>
#include <assert.h>
#include <kl/templates/queue/dummy.h>
#include <kl/macros.h>
#include <stdlib.h>
#include <string.h>


#define KL_DEFINE_QUEUE(NAME, T)                                                                                \
  typedef struct {                                                                                              \
    KL_QueueConfig config;                                                                                      \
    T* items;                                                                                                   \
    KL_Allocation allocation;                                                                                   \
    volatile int64_t n_slots_allocated;                                                                         \
    volatile int64_t length;                                                                                    \
    volatile int64_t current_capacity;                                                                          \
    KL_Mutex lock;                                                                                              \
    bool initialized;                                                                                           \
  } KL_##NAME##_Queue;                                                                                          \
  bool kl_##NAME##_queue_init(KL_##NAME##_Queue* queue, KL_QueueConfig cfg);                                    \
  void kl_##NAME##_queue_destroy(KL_##NAME##_Queue* queue);                                                     \
  void kl_##NAME##_queue_reset(KL_##NAME##_Queue* queue);                                                       \
  bool kl_##NAME##_queue_push(KL_##NAME##_Queue* queue, T item);                                                \
  bool kl_##NAME##_queue_try_push(KL_##NAME##_Queue* queue, T item);                                            \
  bool kl_##NAME##_queue_try_push_timeout(KL_##NAME##_Queue* queue, T item, double timeout_ms);                 \
  bool kl_##NAME##_queue_pop_first(KL_##NAME##_Queue* queue, T* out);                                           \
  bool kl_##NAME##_queue_pop_last(KL_##NAME##_Queue* queue, T* out);                                            \
  bool kl_##NAME##_queue_pop_at(KL_##NAME##_Queue* queue, int64_t idx, T* out);                                 \
  bool kl_##NAME##_queue_try_pop_first(KL_##NAME##_Queue* queue, T* out);                                       \
  bool kl_##NAME##_queue_try_pop_last(KL_##NAME##_Queue* queue, T* out);                                        \
  bool kl_##NAME##_queue_try_pop_at(KL_##NAME##_Queue* queue, int64_t idx, T* out);                             \
  bool kl_##NAME##_queue_try_pop_first_timeout(KL_##NAME##_Queue* queue, T* out, double timeout_ms);            \
  bool kl_##NAME##_queue_try_pop_last_timeout(KL_##NAME##_Queue* queue, T* out, double timeout_ms);             \
  bool kl_##NAME##_queue_try_pop_at_timeout(KL_##NAME##_Queue* queue, int64_t idx, T* out, double timeout_ms);  \
  void kl_##NAME##_queue_grow(KL_##NAME##_Queue *queue, int64_t n_slots);



#define KL_IMPL_QUEUE(NAME, T)                                                                                   \
  bool kl_##NAME##_queue_init(KL_##NAME##_Queue *queue, KL_QueueConfig cfg) {                                    \
    assert(cfg.capacity > 0);                                                                                    \
    if (queue->initialized)                                                                                      \
      return true;                                                                                               \
    queue->config = cfg;                                                                                         \
    queue->items = 0;                                                                                            \
    queue->length = 0;                                                                                           \
    queue->n_slots_allocated = 0;                                                                                \
    queue->current_capacity = cfg.capacity;                                                                      \
    queue->allocation = (KL_Allocation){0};                                                                      \
    kl_mutex_init(&queue->lock);                                                                                 \
    queue->initialized = true;                                                                                   \
    return true;                                                                                                 \
  }                                                                                                              \
  void kl_##NAME##_queue_destroy(KL_##NAME##_Queue *queue) {                                                     \
    assert(queue->initialized == true);                                                                          \
                                                                                                                 \
    kl_mutex_lock(&queue->lock);                                                                                 \
                                                                                                                 \
    if (queue->config.allocator == 0) {                                                                          \
      if (queue->items != 0) {                                                                                   \
        free(queue->items);                                                                                      \
      }                                                                                                          \
    }                                                                                                            \
    queue->items = 0;                                                                                            \
    queue->config = (KL_QueueConfig){0};                                                                         \
    queue->length = 0;                                                                                           \
    queue->n_slots_allocated = 0;                                                                                \
    queue->current_capacity = 0;                                                                                 \
    queue->initialized = false;                                                                                  \
                                                                                                                 \
    kl_mutex_unlock(&queue->lock);                                                                               \
                                                                                                                 \
    kl_mutex_destroy(&queue->lock);                                                                              \
  }                                                                                                              \
  void kl_##NAME##_queue_reset(KL_##NAME##_Queue *queue) {                                                       \
    kl_mutex_lock(&queue->lock);                                                                                 \
                                                                                                                 \
    if (queue->items != 0) {                                                                                     \
      if (queue->n_slots_allocated > 0) {                                                                        \
        memset(&queue->items[0], 0, queue->n_slots_allocated * sizeof(T));                                       \
      }                                                                                                          \
    }                                                                                                            \
    queue->length = 0;                                                                                           \
    kl_mutex_unlock(&queue->lock);                                                                               \
  }                                                                                                              \
  void kl_##NAME##_queue_grow(KL_##NAME##_Queue *queue, int64_t n_slots) {                                       \
    if (queue->config.allocator != 0) {                                                                          \
      queue->items = (T *)kl_allocator_realloc(                                                                  \
          queue->config.allocator,                                                                               \
          (queue->n_slots_allocated + n_slots) * sizeof(T), &queue->allocation);                                 \
      assert(queue->items != 0);                                                                                 \
      memset(&queue->items[queue->n_slots_allocated], 0, n_slots * sizeof(T));                                   \
      queue->n_slots_allocated += n_slots;                                                                       \
      return;                                                                                                    \
    }                                                                                                            \
    queue->items = (T *)realloc(                                                                                 \
        queue->items, (queue->n_slots_allocated + n_slots) * sizeof(T));                                         \
    assert(queue->items != 0);                                                                                   \
    memset(&queue->items[queue->n_slots_allocated], 0, n_slots * sizeof(T));                                     \
    queue->n_slots_allocated += n_slots;                                                                         \
  }                                                                                                              \
  static inline bool kl_##NAME##_queue_push_(KL_##NAME##_Queue *queue, T item) {                                 \
    assert(queue->initialized == true);                                                                          \
    assert(queue->current_capacity > 0);                                                                         \
                                                                                                                 \
    if (queue->config.max_length > 0 && queue->length >= queue->config.max_length) {                             \
      return false;                                                                                              \
    }                                                                                                            \
    int64_t avail = queue->n_slots_allocated - queue->length;                                                    \
    if (avail <= 0) {                                                                                            \
      kl_##NAME##_queue_grow(queue, queue->current_capacity);                                                    \
      queue->current_capacity += KL_MAX(queue->current_capacity / 3, 1);                                         \
    }                                                                                                            \
    assert(queue->items != 0);                                                                                   \
    queue->items[queue->length++] = item;                                                                        \
    return true;                                                                                                 \
  }                                                                                                              \
  bool kl_##NAME##_queue_push(KL_##NAME##_Queue *queue, T item) {                                                \
    kl_mutex_lock(&queue->lock);                                                                                 \
    bool ret = kl_##NAME##_queue_push_(queue, item);                                                             \
    kl_mutex_unlock(&queue->lock);                                                                               \
    return ret;                                                                                                  \
  }                                                                                                              \
  bool kl_##NAME##_queue_try_push(KL_##NAME##_Queue* queue, T item) {                                            \
    assert(queue->initialized);                                                                                  \
    if (!kl_mutex_trylock(&queue->lock)) return false;                                                           \
    bool ret = kl_##NAME##_queue_push_(queue, item);                                                             \
    kl_mutex_unlock(&queue->lock);                                                                               \
    return ret;                                                                                                  \
  }                                                                                                              \
  bool kl_##NAME##_queue_try_push_timeout(KL_##NAME##_Queue* queue, T item, double timeout_ms) {                 \
    assert(queue->initialized);                                                                                  \
    if (!kl_mutex_trylock_timeout(&queue->lock, timeout_ms)) return false;                                       \
    bool ret = kl_##NAME##_queue_push_(queue, item);                                                             \
    kl_mutex_unlock(&queue->lock);                                                                               \
    return ret;                                                                                                  \
  }                                                                                                              \
  static inline bool kl_##NAME##_queue_pop_at_(KL_##NAME##_Queue *queue, int64_t idx, T *out) {                  \
    assert(queue->initialized == true);                                                                          \
    if (queue->length <= 0 || queue->items == 0 || idx < 0) {                                                    \
      return false;                                                                                              \
    }                                                                                                            \
    assert(idx < queue->length);                                                                                 \
    assert(idx >= 0);                                                                                            \
    if (out != 0) {                                                                                              \
      *out = queue->items[idx];                                                                                  \
    }                                                                                                            \
    for (int64_t i = idx; i < queue->length - 1; i++) {                                                          \
      queue->items[i] = queue->items[i + 1];                                                                     \
    }                                                                                                            \
    queue->length -= 1;                                                                                          \
    return true;                                                                                                 \
  }                                                                                                              \
  bool kl_##NAME##_queue_pop_first(KL_##NAME##_Queue* queue, T* out) {                                           \
    kl_mutex_lock(&queue->lock);                                                                                 \
    bool ret = kl_##NAME##_queue_pop_at_(queue, 0, out);                                                         \
    kl_mutex_unlock(&queue->lock);                                                                               \
    return ret;                                                                                                  \
  }                                                                                                              \
  bool kl_##NAME##_queue_pop_last(KL_##NAME##_Queue* queue, T* out) {                                            \
    kl_mutex_lock(&queue->lock);                                                                                 \
    bool ret = kl_##NAME##_queue_pop_at_(queue, queue->length - 1, out);                                         \
    kl_mutex_unlock(&queue->lock);                                                                               \
    return ret;                                                                                                  \
  }                                                                                                              \
  bool kl_##NAME##_queue_pop_at(KL_##NAME##_Queue *queue, int64_t idx, T *out) {                                 \
    kl_mutex_lock(&queue->lock);                                                                                 \
    bool ret = kl_##NAME##_queue_pop_at_(queue, idx, out);                                                       \
    kl_mutex_unlock(&queue->lock);                                                                               \
    return ret;                                                                                                  \
  }                                                                                                              \
  bool kl_##NAME##_queue_try_pop_first(KL_##NAME##_Queue* queue, T* out) {                                       \
    if (!kl_mutex_trylock(&queue->lock)) return false;                                                           \
    bool ret = kl_##NAME##_queue_pop_at_(queue, 0, out);                                                         \
    kl_mutex_unlock(&queue->lock);                                                                               \
    return ret;                                                                                                  \
  }                                                                                                              \
  bool kl_##NAME##_queue_try_pop_last(KL_##NAME##_Queue* queue, T* out) {                                        \
    if (!kl_mutex_trylock(&queue->lock)) return false;                                                           \
    bool ret = kl_##NAME##_queue_pop_at_(queue, queue->length - 1, out);                                         \
    kl_mutex_unlock(&queue->lock);                                                                               \
    return ret;                                                                                                  \
  }                                                                                                              \
  bool kl_##NAME##_queue_try_pop_at(KL_##NAME##_Queue* queue, int64_t idx, T* out) {                             \
    if (!kl_mutex_trylock(&queue->lock)) return false;                                                           \
    bool ret = kl_##NAME##_queue_pop_at_(queue, idx, out);                                                       \
    kl_mutex_unlock(&queue->lock);                                                                               \
    return ret;                                                                                                  \
  }                                                                                                              \
  bool kl_##NAME##_queue_try_pop_first_timeout(KL_##NAME##_Queue* queue, T* out, double timeout_ms) {            \
    if (!kl_mutex_trylock_timeout(&queue->lock, timeout_ms)) return false;                                       \
    bool ret = kl_##NAME##_queue_pop_at_(queue, 0, out);                                                         \
    kl_mutex_unlock(&queue->lock);                                                                               \
    return ret;                                                                                                  \
  }                                                                                                              \
  bool kl_##NAME##_queue_try_pop_last_timeout(KL_##NAME##_Queue* queue, T* out, double timeout_ms) {             \
    if (!kl_mutex_trylock_timeout(&queue->lock, timeout_ms)) return false;                                       \
    bool ret = kl_##NAME##_queue_pop_at_(queue, queue->length - 1, out);                                         \
    kl_mutex_unlock(&queue->lock);                                                                               \
    return ret;                                                                                                  \
  }                                                                                                              \
  bool kl_##NAME##_queue_try_pop_at_timeout(KL_##NAME##_Queue* queue, int64_t idx, T* out, double timeout_ms) {  \
    if (!kl_mutex_trylock_timeout(&queue->lock, timeout_ms)) return false;                                       \
    bool ret = kl_##NAME##_queue_pop_at_(queue, idx, out);                                                       \
    kl_mutex_unlock(&queue->lock);                                                                               \
    return ret;                                                                                                  \
  }

/* generated */
