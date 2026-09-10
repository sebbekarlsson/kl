#include <kl/debug/profiler.h>
#include <kl/time/time.h>
#include <kl/macros.h>
#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include <stdalign.h>
#include <unistd.h>

KL_IMPL_LIST(profiler_message, KL_ProfilerMessage);

#define KL_PROFILER_DEFAULT_MAX_QUEUE_SIZE (1024 * 4)
#define KL_PROFILER_DEFAULT_MAX_BLOCK_DEPTH 64
#define KL_PROFILER_DEFAULT_MAX_THREADS 16
#define KL_PROFILER_DEFAULT_POLL_INTERVAL_US 1000

// ---------------------------------------------------------------------------
// Per-thread block stacks
// ---------------------------------------------------------------------------

static int64_t kl_profiler_stack_size(const KL_ProfilerConfig* cfg) {
  return (int64_t)sizeof(KL_ProfilerBlockStack)
       + cfg->max_block_depth * (int64_t)sizeof(KL_ProfilerFrame);
}

// Runs when a thread that used the profiler exits, giving its stack slot back.
// Never runs after kl_profiler_destroy: destroy deletes the key first, and
// POSIX guarantees a deleted key's destructor is not invoked.
static void kl_profiler_stack_destructor(void* ptr) {
  KL_ProfilerBlockStack* stack = (KL_ProfilerBlockStack*)ptr;
  if (stack == 0) return;
  KL_Profiler* prof = stack->owner;
  if (prof == 0 || !prof->initialized) return;
  kl_binary_pool_release(&prof->stacks.pool, stack->handle);
}

// The calling thread's stack. With `create`, a stack is acquired from the pool
// on first use; otherwise 0 is returned for a thread that never began a block.
static KL_ProfilerBlockStack* kl_profiler_get_stack(KL_Profiler* prof, bool create) {
  KL_ProfilerBlockStack* stack = (KL_ProfilerBlockStack*)pthread_getspecific(prof->stacks.key);
  if (stack != 0 || !create) return stack;

  uint8_t* raw = 0;
  int64_t handle = kl_binary_pool_acquire(&prof->stacks.pool, &raw);
  if (handle < 0 || raw == 0) return 0;

  stack = (KL_ProfilerBlockStack*)raw;
  memset(stack, 0, (size_t)kl_profiler_stack_size(&prof->config));
  stack->owner = prof;
  stack->handle = handle;
  stack->depth = 0;
  stack->thread_id = (uint64_t)pthread_self();

  if (pthread_setspecific(prof->stacks.key, stack) != 0) {
    kl_binary_pool_release(&prof->stacks.pool, handle);
    return 0;
  }

  return stack;
}

// ---------------------------------------------------------------------------
// Output
// ---------------------------------------------------------------------------

static void kl_profiler_write_json_string(FILE* fp, const char* str) {
  fputc('"', fp);
  for (const unsigned char* c = (const unsigned char*)str; *c != 0; c++) {
    switch (*c) {
      case '"':  fputs("\\\"", fp); break;
      case '\\': fputs("\\\\", fp); break;
      case '\n': fputs("\\n", fp); break;
      case '\r': fputs("\\r", fp); break;
      case '\t': fputs("\\t", fp); break;
      case '\b': fputs("\\b", fp); break;
      case '\f': fputs("\\f", fp); break;
      default:
        if (*c < 0x20) fprintf(fp, "\\u%04x", (unsigned)*c);
        else fputc((int)*c, fp);
    }
  }
  fputc('"', fp);
}

// One JSON object per line (jsonl).
static void kl_profiler_write_point(const KL_ProfilerPoint* point, FILE* fp) {
  fputs("{ \"label\": ", fp);
  kl_profiler_write_json_string(fp, point->label);
  fprintf(fp, ", \"timestamp\": %.6f, \"duration\": %.6f, \"depth\": %d, \"thread\": %llu }\n",
          point->timestamp, point->duration, (int)point->depth,
          (unsigned long long)point->thread_id);
}

// Swap the pending buffer out under the lock, then write it without holding
// the lock. Returns the number of points written.
static int64_t kl_profiler_drain(KL_Profiler* prof) {
  kl_mutex_lock(&prof->messages.lock);
  KL_profiler_message_List tmp = prof->messages.pending;
  prof->messages.pending = prof->messages.draining;
  prof->messages.draining = tmp;
  kl_mutex_unlock(&prof->messages.lock);

  KL_profiler_message_List* batch = &prof->messages.draining;
  int64_t count = batch->length;
  if (count <= 0) return 0;

  for (int64_t i = 0; i < count; i++) {
    KL_ProfilerMessage* msg = &batch->items[i];
    switch (msg->type) {
      case KL_PROFILER_MESSAGE_POINT:
        kl_profiler_write_point(&msg->as.point, prof->output.fp);
        break;
      case KL_PROFILER_MESSAGE_NONE:
        break;
    }
  }
  fflush(prof->output.fp);
  kl_profiler_message_list_reset_fast(batch);

  // Published only after fflush, so a reader that observes `written` can rely
  // on the bytes being in the file.
  atomic_fetch_add(&prof->stats.written, count);
  return count;
}

static void* kl_profiler_thread_fn(void* ptr) {
  assert(ptr != 0);
  KL_Profiler* prof = (KL_Profiler*)ptr;
  assert(prof->initialized);

  useconds_t interval = (useconds_t)prof->config.poll_interval_us;

  while (atomic_load(&prof->thread.running)) {
    if (kl_profiler_drain(prof) == 0) {
      usleep(interval);
    }
  }

  // Everything submitted before kill() flipped `running` is still pending;
  // write it so kill() can promise it is on disk.
  kl_profiler_drain(prof);
  return 0;
}

// ---------------------------------------------------------------------------
// Lifecycle
// ---------------------------------------------------------------------------

static FILE* kl_profiler_open_output(KL_StrConst filepath) {
  // KL_StrConst is not guaranteed to be NUL-terminated.
  char* path = (char*)malloc((size_t)filepath.length + 1);
  if (path == 0) return 0;
  memcpy(path, filepath.value, (size_t)filepath.length);
  path[filepath.length] = 0;

  FILE* fp = fopen(path, "w");
  free(path);
  return fp;
}

bool kl_profiler_init(KL_Profiler* prof, KL_ProfilerConfig cfg) {
  if (prof->initialized) return true;
  assert(cfg.filepath.length > 0);
  assert(cfg.filepath.value != 0);

  if (cfg.max_queue_size <= 0) cfg.max_queue_size = KL_PROFILER_DEFAULT_MAX_QUEUE_SIZE;
  if (cfg.max_block_depth <= 0) cfg.max_block_depth = KL_PROFILER_DEFAULT_MAX_BLOCK_DEPTH;
  if (cfg.max_threads <= 0) cfg.max_threads = KL_PROFILER_DEFAULT_MAX_THREADS;
  if (cfg.poll_interval_us <= 0) cfg.poll_interval_us = KL_PROFILER_DEFAULT_POLL_INTERVAL_US;

  FILE* fp = kl_profiler_open_output(cfg.filepath);
  if (fp == 0) return false;

  memset(prof, 0, sizeof(KL_Profiler));
  prof->config = cfg;
  prof->output.fp = fp;

  if (pthread_key_create(&prof->stacks.key, kl_profiler_stack_destructor) != 0) {
    fclose(fp);
    memset(prof, 0, sizeof(KL_Profiler));
    return false;
  }

  kl_mutex_init(&prof->messages.lock);

  kl_profiler_message_list_init(&prof->messages.pending, (KL_ListConfig){
    .capacity = cfg.max_queue_size,
  });
  kl_profiler_message_list_init(&prof->messages.draining, (KL_ListConfig){
    .capacity = cfg.max_queue_size,
  });

  kl_binary_pool_init(&prof->stacks.pool, (KL_BinaryPoolConfig){
    .count = cfg.max_threads,
    .element_size = kl_profiler_stack_size(&cfg),
    .element_alignment = alignof(KL_ProfilerBlockStack),
    .grow_when_full = true,
  });

  atomic_store(&prof->thread.running, false);
  atomic_store(&prof->stats.submitted, 0);
  atomic_store(&prof->stats.written, 0);
  atomic_store(&prof->stats.dropped, 0);

  prof->initialized = true;
  return true;
}

void kl_profiler_destroy(KL_Profiler* prof) {
  assert(prof->initialized);

  kl_profiler_kill(prof);

  // Points recorded while no writer thread was running (or after it stopped).
  while (kl_profiler_drain(prof) > 0) {}

  // Delete the key before the pool goes away so no exiting thread can release
  // into freed memory. Stacks still held by live threads die with the pool.
  pthread_key_delete(prof->stacks.key);
  kl_binary_pool_destroy(&prof->stacks.pool);

  kl_profiler_message_list_destroy(&prof->messages.pending);
  kl_profiler_message_list_destroy(&prof->messages.draining);
  kl_mutex_destroy(&prof->messages.lock);

  fclose(prof->output.fp);

  memset(prof, 0, sizeof(KL_Profiler));
}

bool kl_profiler_start(KL_Profiler* prof) {
  assert(prof->initialized);
  if (atomic_load(&prof->thread.running)) return true;

  atomic_store(&prof->thread.running, true);
  if (pthread_create(&prof->thread.id, 0, kl_profiler_thread_fn, prof) != 0) {
    atomic_store(&prof->thread.running, false);
    return false;
  }

  return true;
}

void kl_profiler_kill(KL_Profiler* prof) {
  assert(prof->initialized);
  if (!atomic_load(&prof->thread.running)) return;

  atomic_store(&prof->thread.running, false);
  pthread_join(prof->thread.id, 0);
}

bool kl_profiler_flush(KL_Profiler* prof) {
  assert(prof->initialized);
  if (!atomic_load(&prof->thread.running)) return false;

  int64_t target = atomic_load(&prof->stats.submitted);
  useconds_t interval = (useconds_t)KL_MIN(prof->config.poll_interval_us, 100);

  while (atomic_load(&prof->stats.written) < target) {
    // If the writer is stopped underneath us its final drain covers `target`,
    // but a stopped writer can't make further progress, so don't spin forever.
    if (!atomic_load(&prof->thread.running)) {
      return atomic_load(&prof->stats.written) >= target;
    }
    usleep(interval);
  }

  return true;
}

// ---------------------------------------------------------------------------
// Blocks
// ---------------------------------------------------------------------------

bool kl_profiler_begin_block(KL_Profiler* prof, KL_StrConst name) {
  assert(prof->initialized);
  assert(name.length > 0);
  assert(name.value != 0);

  KL_ProfilerBlockStack* stack = kl_profiler_get_stack(prof, true);
  if (stack == 0) return false;

  // Depth is always bumped, even past capacity, so a matching end_block pops
  // the right frame no matter how deep the nesting went.
  int64_t depth = stack->depth++;
  if (depth >= prof->config.max_block_depth) return false;

  stack->frames[depth] = (KL_ProfilerFrame){
    .timestamp = kl_time_now_ms__monotonic(),
    .name = name,
  };
  return true;
}

static bool kl_profiler_submit(KL_Profiler* prof, KL_ProfilerMessage msg) {
  kl_mutex_lock(&prof->messages.lock);
  bool accepted = prof->messages.pending.length < prof->config.max_queue_size;
  if (accepted) {
    kl_profiler_message_list_push(&prof->messages.pending, msg);
    atomic_fetch_add(&prof->stats.submitted, 1);
  }
  kl_mutex_unlock(&prof->messages.lock);

  if (!accepted) atomic_fetch_add(&prof->stats.dropped, 1);
  return accepted;
}

bool kl_profiler_end_block(KL_Profiler* prof) {
  assert(prof->initialized);

  KL_ProfilerBlockStack* stack = kl_profiler_get_stack(prof, false);
  if (stack == 0 || stack->depth <= 0) return false; // unbalanced end

  double now = kl_time_now_ms__monotonic();

  int64_t depth = --stack->depth;
  if (depth >= prof->config.max_block_depth) return false; // was never recorded

  KL_ProfilerFrame frame = stack->frames[depth];

  KL_ProfilerMessage msg = {
    .type = KL_PROFILER_MESSAGE_POINT,
    .as.point = {
      .timestamp = frame.timestamp,
      .duration = now - frame.timestamp,
      .depth = (int32_t)depth,
      .thread_id = stack->thread_id,
    },
  };

  int64_t label_length = 0;
  if (frame.name.value != 0 && frame.name.length > 0) {
    label_length = KL_MIN(frame.name.length, (int64_t)(KL_PROFILER_LABEL_CAP - 1));
    memcpy(msg.as.point.label, frame.name.value, (size_t)label_length);
  }
  msg.as.point.label[label_length] = 0;

  return kl_profiler_submit(prof, msg);
}

int64_t kl_profiler_depth(KL_Profiler* prof) {
  assert(prof->initialized);
  KL_ProfilerBlockStack* stack = kl_profiler_get_stack(prof, false);
  return stack != 0 ? stack->depth : 0;
}
