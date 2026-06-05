#include <kl/templates/ringbuffer/dummy.h>
#include <kl/memory/alloc.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>
#include <stdalign.h>

void kl_NAME_ringbuffer_init(KL_NAME_RingBuffer* rb, KL_RingBufferConfig cfg) {
  if (rb->initialized) return;
  assert(cfg.length > 0);
  rb->config = cfg;

  if (cfg.allocator != 0) {
    rb->items = (T_X*)kl_allocator_malloc_aligned(cfg.allocator, cfg.length * sizeof(T_X), alignof(T_X));
  } else {
    rb->items = (T_X*)kl_aligned_alloc(cfg.length * sizeof(T_X), alignof(T_X));
  }

  assert(rb->items != 0);

  rb->length = cfg.length;
  rb->initialized = true;
}

void kl_NAME_ringbuffer_destroy(KL_NAME_RingBuffer* rb) {
  if (!rb->initialized) return;

  if (rb->config.allocator == 0) {
    if (rb->items != 0) {
      // we allocated the data so we need to free it.
      free(rb->items);
    }
  }

  rb->items = 0;
  memset(rb, 0, sizeof(KL_NAME_RingBuffer));
}

void kl_NAME_ringbuffer_reset(KL_NAME_RingBuffer* rb) {
  assert(rb->initialized);

  if (rb->items != 0) {
    assert(rb->length > 0);
    memset(rb->items, 0, rb->length * sizeof(T_X));
  }

  rb->cursor_write = 0;
  rb->cursor_read = 0;
}

void kl_NAME_ringbuffer_write(KL_NAME_RingBuffer* rb, T_X value) {
  assert(rb->initialized);
  assert(rb->items != 0);
  assert(rb->length > 0);
  assert(rb->cursor_write >= 0);
  assert(rb->cursor_write < rb->length);

  memcpy(&rb->items[rb->cursor_write], &value, sizeof(T_X));

  rb->cursor_write = (rb->cursor_write + 1) % rb->length;
}

T_X kl_NAME_ringbuffer_read(KL_NAME_RingBuffer* rb) {
  assert(rb->initialized);

  assert(rb->initialized);
  assert(rb->items != 0);
  assert(rb->length > 0);
  assert(rb->cursor_read >= 0);
  assert(rb->cursor_read < rb->length);

  T_X value = rb->items[rb->cursor_read];

  rb->cursor_read = (rb->cursor_read + 1) % rb->length;

  return value;
}
