/*
This file was automatically generated and must not be modified directly.
*/


#include <stdbool.h>
#include <stdint.h>
#include <kl/memory/allocator.h>
#include <kl/templates/ringbuffer/shared.h>
#include <kl/templates/x__shared.h>
#include <kl/templates/ringbuffer/dummy.h>
#include <kl/memory/alloc.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdalign.h>


#define KL_DEFINE_RINGBUFFER(NAME, T)                                                     \
  typedef struct {                                                                        \
    KL_RingBufferConfig config;                                                           \
    bool initialized;                                                                     \
    int64_t length;                                                                       \
    T* items;                                                                             \
    int64_t cursor_read;                                                                  \
    int64_t cursor_write;                                                                 \
  } KL_##NAME##_RingBuffer;                                                               \
  void kl_##NAME##_ringbuffer_init(KL_##NAME##_RingBuffer* rb, KL_RingBufferConfig cfg);  \
  void kl_##NAME##_ringbuffer_destroy(KL_##NAME##_RingBuffer* rb);                        \
  void kl_##NAME##_ringbuffer_reset(KL_##NAME##_RingBuffer* rb);                          \
  void kl_##NAME##_ringbuffer_write(KL_##NAME##_RingBuffer* rb, T value);                 \
  T kl_##NAME##_ringbuffer_read(KL_##NAME##_RingBuffer* rb);



#define KL_IMPL_RINGBUFFER(NAME, T)                                                                    \
  void kl_##NAME##_ringbuffer_init(KL_##NAME##_RingBuffer* rb, KL_RingBufferConfig cfg) {              \
    if (rb->initialized) return;                                                                       \
    assert(cfg.length > 0);                                                                            \
    rb->config = cfg;                                                                                  \
    if (cfg.allocator != 0) {                                                                          \
      rb->items = (T*)kl_allocator_malloc_aligned(cfg.allocator, cfg.length * sizeof(T), alignof(T));  \
    } else {                                                                                           \
      rb->items = (T*)kl_aligned_alloc(cfg.length * sizeof(T), alignof(T));                            \
    }                                                                                                  \
    assert(rb->items != 0);                                                                            \
    rb->length = cfg.length;                                                                           \
    rb->initialized = true;                                                                            \
  }                                                                                                    \
  void kl_##NAME##_ringbuffer_destroy(KL_##NAME##_RingBuffer* rb) {                                    \
    if (!rb->initialized) return;                                                                      \
    if (rb->config.allocator == 0) {                                                                   \
      if (rb->items != 0) {                                                                            \
        free(rb->items);                                                                               \
      }                                                                                                \
    }                                                                                                  \
    rb->items = 0;                                                                                     \
    memset(rb, 0, sizeof(KL_##NAME##_RingBuffer));                                                     \
  }                                                                                                    \
  void kl_##NAME##_ringbuffer_reset(KL_##NAME##_RingBuffer* rb) {                                      \
    assert(rb->initialized);                                                                           \
    if (rb->items != 0) {                                                                              \
      assert(rb->length > 0);                                                                          \
      memset(rb->items, 0, rb->length * sizeof(T));                                                    \
    }                                                                                                  \
    rb->cursor_write = 0;                                                                              \
    rb->cursor_read = 0;                                                                               \
  }                                                                                                    \
  void kl_##NAME##_ringbuffer_write(KL_##NAME##_RingBuffer* rb, T value) {                             \
    assert(rb->initialized);                                                                           \
    assert(rb->items != 0);                                                                            \
    assert(rb->length > 0);                                                                            \
    assert(rb->cursor_write >= 0);                                                                     \
    assert(rb->cursor_write < rb->length);                                                             \
    memcpy(&rb->items[rb->cursor_write], &value, sizeof(T));                                           \
    rb->cursor_write = (rb->cursor_write + 1) % rb->length;                                            \
  }                                                                                                    \
  T kl_##NAME##_ringbuffer_read(KL_##NAME##_RingBuffer* rb) {                                          \
    assert(rb->initialized);                                                                           \
    assert(rb->initialized);                                                                           \
    assert(rb->items != 0);                                                                            \
    assert(rb->length > 0);                                                                            \
    assert(rb->cursor_read >= 0);                                                                      \
    assert(rb->cursor_read < rb->length);                                                              \
    T value = rb->items[rb->cursor_read];                                                              \
    rb->cursor_read = (rb->cursor_read + 1) % rb->length;                                              \
    return value;                                                                                      \
  }

/* generated */
