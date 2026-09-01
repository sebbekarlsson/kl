#ifndef KL_T_X_QUEUE_H
#define KL_T_X_QUEUE_H
#include <stdint.h>
#include <stdint.h>
#include <stdbool.h>
#include <kl/memory/allocator.h>
#include <kl/threads/mutex.h>
#include <kl/templates/x__shared.h>
#include <kl/templates/queue/shared.h>

typedef struct {
  KL_QueueConfig config;
  T_X* items;
  KL_Allocation allocation;
  volatile int64_t n_slots_allocated;
  volatile int64_t length;
  volatile int64_t current_capacity;
  KL_Mutex lock;
  bool initialized;
} KL_NAME_Queue;

bool kl_NAME_queue_init(KL_NAME_Queue* queue, KL_QueueConfig cfg);

void kl_NAME_queue_destroy(KL_NAME_Queue* queue);

void kl_NAME_queue_reset(KL_NAME_Queue* queue);

bool kl_NAME_queue_push(KL_NAME_Queue* queue, T_X item);

bool kl_NAME_queue_try_push(KL_NAME_Queue* queue, T_X item);

bool kl_NAME_queue_try_push_timeout(KL_NAME_Queue* queue, T_X item, double timeout_ms);

bool kl_NAME_queue_pop_first(KL_NAME_Queue* queue, T_X* out);

bool kl_NAME_queue_pop_last(KL_NAME_Queue* queue, T_X* out);

bool kl_NAME_queue_pop_at(KL_NAME_Queue* queue, int64_t idx, T_X* out);

bool kl_NAME_queue_try_pop_first(KL_NAME_Queue* queue, T_X* out);

bool kl_NAME_queue_try_pop_last(KL_NAME_Queue* queue, T_X* out);

bool kl_NAME_queue_try_pop_at(KL_NAME_Queue* queue, int64_t idx, T_X* out);


bool kl_NAME_queue_try_pop_first_timeout(KL_NAME_Queue* queue, T_X* out, double timeout_ms);

bool kl_NAME_queue_try_pop_last_timeout(KL_NAME_Queue* queue, T_X* out, double timeout_ms);

bool kl_NAME_queue_try_pop_at_timeout(KL_NAME_Queue* queue, int64_t idx, T_X* out, double timeout_ms);


void kl_NAME_queue_grow(KL_NAME_Queue *queue, int64_t n_slots);
#endif
