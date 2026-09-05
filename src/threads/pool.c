#include <kl/threads/pool.h>
#include <kl/macros.h>
#include <kl/string/string.h>
#include <string.h>
#include <assert.h>
#include <stdlib.h>
#include <stdio.h>
#include <pthread.h>

void kl_worker_pool_init(KL_WorkerPool* pool, KL_WorkerPoolConfig cfg) {
  if (pool->initialized) return;
  assert(cfg.max_workers > 0);
  assert(cfg.approx_payload_size > 0);

  if (cfg.get_time_fn == 0) {
    cfg.get_time_fn = kl_time_now_s__monotonic;
  }

  pool->config = cfg;

  kl_binary_pool_init(&pool->workers.pool, (KL_BinaryPoolConfig){
    .count = cfg.max_workers,
    .element_size = sizeof(KL_Worker),
    .element_alignment = alignof(KL_Worker),
    .grow_when_full = false,
  });

  kl_mutex_init(&pool->workers.lock);

  kl_allocator_init(&pool->workers.payload_allocator, (KL_AllocatorConfig){
    .capacity = cfg.max_workers * cfg.approx_payload_size + 16,
    .track_allocations = true,
    .tracked_allocations_capacity= cfg.max_workers + 1,
  });

  pool->initialized = true;
}

void kl_worker_pool_destroy(KL_WorkerPool* pool) {
  assert(pool->initialized);

  kl_worker_pool_stop_all_workers(pool);
  
  kl_allocator_destroy(&pool->workers.payload_allocator);
  kl_mutex_destroy(&pool->workers.lock);
  kl_binary_pool_destroy(&pool->workers.pool);
  memset(pool, 0, sizeof(KL_WorkerPool));
}

void kl_worker_pool_reset(KL_WorkerPool* pool) {
  assert(pool->initialized);

  kl_worker_pool_stop_all_workers(pool);

  kl_mutex_lock(&pool->workers.lock);

  kl_allocator_reset(&pool->workers.payload_allocator);
  kl_binary_pool_reset(&pool->workers.pool);
  
  kl_mutex_unlock(&pool->workers.lock);
}

KL_Worker* kl_worker_pool_get_worker(KL_WorkerPool* pool, int64_t id) {
  assert(pool->initialized);
  KL_Worker* worker = (KL_Worker*)kl_binary_pool_get(&pool->workers.pool, id);
  if (worker == 0) return 0;
  assert(worker->id == id);
  return worker;
}

int64_t kl_worker_pool__worker_register(KL_WorkerPool* pool, KL_WorkerConfig cfg) {
  assert(pool->initialized);
  assert(pool->config.get_time_fn != 0);
  assert(cfg.fn != 0);

  if (pool->workers.pool.taken_indices.length >= pool->workers.pool.config.count) {
    printf("%ld >= %ld\n", pool->workers.pool.taken_indices.length, pool->workers.pool.config.count);
  }

  kl_mutex_lock(&pool->workers.lock);

  uint8_t* raw = 0;
  int64_t id = kl_binary_pool_acquire(&pool->workers.pool, &raw);
  assert(raw != 0);
  assert(id >= 0);

  KL_Worker* worker = (KL_Worker*)raw;
  memset(worker, 0, sizeof(KL_Worker));

  worker->id = id;
  worker->registered_at = pool->config.get_time_fn();
  worker->fn = cfg.fn;

  if (cfg.name.length > 0 && cfg.name.value != 0) {
    worker->name = kl_str_const_strdup(cfg.name, &pool->workers.payload_allocator);
    assert(worker->name.value != 0);
    assert(worker->name.length == cfg.name.length);
  }


  if (cfg.payload.size > 0 && cfg.payload.value != 0) {
    worker->payload.value = kl_allocator_malloc_tracked_aligned(&pool->workers.payload_allocator, cfg.payload.size, cfg.payload.alignment, &worker->payload_allocation);
    assert(worker->payload.value != 0);
    worker->payload.size = cfg.payload.size;
    memcpy(worker->payload.value, cfg.payload.value, cfg.payload.size);
  }

  kl_mutex_unlock(&pool->workers.lock);

  return worker->id;
}

void kl_worker_pool__worker_unregister(KL_WorkerPool* pool, int64_t id) {
  assert(pool->initialized);
  assert(id >= 0);

  kl_mutex_lock(&pool->workers.lock);

  if (!kl_worker_pool__worker_stop(pool, id)) {
    kl_mutex_unlock(&pool->workers.lock);
    return;
  }

  KL_Worker* worker = kl_worker_pool_get_worker(pool, id);

  if (worker == 0) {
    kl_mutex_unlock(&pool->workers.lock);
    return;
  }

  assert(!worker->thread.running);

  if (worker->payload_allocation.size > 0) {
    kl_allocator_free_allocation(&pool->workers.payload_allocator, worker->payload_allocation);
  }

  kl_binary_pool_release(&pool->workers.pool, id);
  
  kl_mutex_unlock(&pool->workers.lock);
}

// returns false if the worker doesn't exist
bool kl_worker_pool__worker_get_status(KL_WorkerPool* pool, int64_t id, KL_WorkerStatus* out) {
  assert(pool->initialized);
  assert(id >= 0);
  assert(pool->config.get_time_fn != 0);
  
  KL_Worker* worker = kl_worker_pool_get_worker(pool, id);
  if (worker == 0) {
    return false;
  } 

  double now = pool->config.get_time_fn();

  out->time_existing = now - worker->registered_at;
  out->time_running = now - worker->started_at;
  out->running = worker->thread.running;

  return true;
}


static inline void* worker_thread_fn(void* ptr) {
  assert(ptr != 0);
  KL_Worker* worker = (KL_Worker*)ptr;
  assert(worker->fn != 0);
  void* ret = worker->fn(worker);
  return ret;
}

bool kl_worker_pool__worker_dispatch(KL_WorkerPool* pool, int64_t id) {
  assert(pool->initialized);
  assert(pool->config.get_time_fn != 0);

  KL_Worker* worker = kl_worker_pool_get_worker(pool, id);

  if (worker == 0 || worker->thread.running) {
    return false;
  }

  assert(worker->fn != 0);
  worker->thread.running = true;
  worker->started_at = pool->config.get_time_fn();

  if (pthread_create(&worker->thread.id, 0, worker_thread_fn, worker) != 0) {
    worker->thread.running = false;
    return false;
  }

  return true;
}

bool kl_worker_pool__worker_stop(KL_WorkerPool* pool, int64_t id) {
  assert(pool->initialized);


  KL_Worker* worker = kl_worker_pool_get_worker(pool, id);

  if (worker == 0 || !worker->thread.running) {
    return false;
  }

  worker->thread.running = false;

  return true;
}

bool kl_worker_pool__worker_join(KL_WorkerPool* pool, int64_t id, void** returns) {
  assert(pool->initialized);

  KL_Worker* worker = kl_worker_pool_get_worker(pool, id);

  if (worker == 0 || !worker->thread.running) {
    return false;
  }

  pthread_join(worker->thread.id, returns);

  return true;
}

int64_t kl_worker_pool_dispatch_all_workers(KL_WorkerPool* pool) {
  assert(pool->initialized);

  int64_t num_oks = 0;
  
  for (int64_t i = 0; i < pool->workers.pool.taken_indices.length; i++) {
    int64_t idx = pool->workers.pool.taken_indices.items[i];
    KL_Worker* worker = (KL_Worker*)kl_binary_pool_get_slot(&pool->workers.pool, idx);
    assert(worker != 0);

    if (kl_worker_pool__worker_dispatch(pool, worker->id)) {
      num_oks += 1;
    }
  }

  return num_oks;
}

int64_t kl_worker_pool_stop_all_workers(KL_WorkerPool* pool) { 
  assert(pool->initialized);

  int64_t num_oks = 0;
  
  for (int64_t i = 0; i < pool->workers.pool.taken_indices.length; i++) {
    int64_t idx = pool->workers.pool.taken_indices.items[i];
    KL_Worker* worker = (KL_Worker*)kl_binary_pool_get_slot(&pool->workers.pool, idx);
    assert(worker != 0);

    if (kl_worker_pool__worker_stop(pool, worker->id)) {
      num_oks += 1;
    }
  }

  return num_oks;
}

// join all workers
int64_t kl_worker_pool_join(KL_WorkerPool* pool) {
  assert(pool->initialized);

  int64_t num_oks = 0;

  for (int64_t i = 0; i < pool->workers.pool.taken_indices.length; i++) {
    int64_t idx = pool->workers.pool.taken_indices.items[i];
    KL_Worker* worker = (KL_Worker*)kl_binary_pool_get_slot(&pool->workers.pool, idx);
    assert(worker != 0);
    if (kl_worker_pool__worker_join(pool, worker->id, 0)) {
      num_oks += 1;
    }
  }

  return num_oks;
}
