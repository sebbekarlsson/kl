#ifndef KL_WORKER_H
#define KL_WORKER_H

#include <stdint.h>
#include <stdbool.h>
#include <pthread.h>
#include <stdatomic.h>
#include <kl/binary/blob.h>
#include <kl/templates/list/list.h>
#include <kl/string/string.h>

struct KL_Worker;

typedef void* (*KL_WorkerFunction)(struct KL_Worker* worker);

typedef struct {
  KL_StrConst name;
  KL_BlobAligned payload;
  KL_WorkerFunction fn;
} KL_WorkerConfig;

typedef struct {
  bool running;
  double time_running; // time elapsed since start
  double time_existing;
} KL_WorkerStatus;

typedef struct KL_Worker {
  int64_t id;
  KL_StrConst name;
  double started_at;
  double registered_at;
  struct {
    pthread_t id;
    atomic_bool running;
  } thread;
  KL_Blob payload;
  KL_Allocation payload_allocation;
  KL_WorkerFunction fn;
} KL_Worker;

KL_DEFINE_LIST(worker, KL_Worker);

#endif
