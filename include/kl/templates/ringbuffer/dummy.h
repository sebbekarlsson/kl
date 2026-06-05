#ifndef KL_RINGBUFFER_H
#define KL_RINGBUFFER_H

#include <stdbool.h>
#include <stdint.h>
#include <kl/memory/allocator.h>
#include <kl/templates/ringbuffer/shared.h>
#include <kl/templates/x__shared.h>


typedef struct {
  KL_RingBufferConfig config;
  bool initialized;
  int64_t length;
  T_X* items;

  int64_t cursor_read;
  int64_t cursor_write;
} KL_NAME_RingBuffer;

void kl_NAME_ringbuffer_init(KL_NAME_RingBuffer* rb, KL_RingBufferConfig cfg);
void kl_NAME_ringbuffer_destroy(KL_NAME_RingBuffer* rb);
void kl_NAME_ringbuffer_reset(KL_NAME_RingBuffer* rb);
void kl_NAME_ringbuffer_write(KL_NAME_RingBuffer* rb, T_X value);
T_X kl_NAME_ringbuffer_read(KL_NAME_RingBuffer* rb);

#endif
