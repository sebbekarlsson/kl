#ifndef KL_WORKER_POOL_H
#define KL_WORKER_POOL_H

#include <stdint.h>
#include <stdbool.h>

#include <kl/threads/worker.h>
#include <kl/threads/mutex.h>
#include <kl/binary/blob.h>
#include <kl/memory/allocator.h>
#include <kl/templates/pool/binary_pool.h>
#include <kl/time/time.h>

typedef struct {
  int64_t max_workers;
  int64_t approx_payload_size;
  KL_GetTimeFunction get_time_fn;
} KL_WorkerPoolConfig;


typedef struct {
  bool initialized;
  KL_WorkerPoolConfig config;
  struct {
    KL_BinaryPool pool;
    KL_Allocator payload_allocator;
    KL_Mutex lock;
  } workers;
} KL_WorkerPool;

void kl_worker_pool_init(KL_WorkerPool* pool, KL_WorkerPoolConfig cfg);

void kl_worker_pool_destroy(KL_WorkerPool* pool);

void kl_worker_pool_reset(KL_WorkerPool* pool);

KL_Worker* kl_worker_pool_get_worker(KL_WorkerPool* pool, int64_t id);

int64_t kl_worker_pool__worker_register(KL_WorkerPool* pool, KL_WorkerConfig cfg);

void kl_worker_pool__worker_unregister(KL_WorkerPool* pool, int64_t id);

// returns false if the worker doesn't exist
bool kl_worker_pool__worker_get_status(KL_WorkerPool* pool, int64_t id, KL_WorkerStatus* out);

bool kl_worker_pool__worker_dispatch(KL_WorkerPool* pool, int64_t id);

bool kl_worker_pool__worker_stop(KL_WorkerPool* pool, int64_t id);

bool kl_worker_pool__worker_join(KL_WorkerPool* pool, int64_t id, void** returns);

/**
* Returns the number of workers that started
**/
int64_t kl_worker_pool_dispatch_all_workers(KL_WorkerPool* pool);

/**
* Returns the number of workers that stopped
**/
int64_t kl_worker_pool_stop_all_workers(KL_WorkerPool* pool);

/**
* Returns the number of workers that joined
**/
int64_t kl_worker_pool_join(KL_WorkerPool* pool);
#endif
