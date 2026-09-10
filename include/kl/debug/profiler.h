/**
* A lightweight, thread-safe block profiler.
*
* Usage:
*
*   kl_profiler_begin_block(&prof, KL_STR_CONST_LIT("update"));
*   ...
*   kl_profiler_end_block(&prof);   // ends the innermost open block on *this* thread
*
* Every thread that calls begin_block gets its own block stack (looked up through
* a pthread key), so blocks nest per thread and end_block never needs an id.
* Finished blocks become points that are appended to a pending buffer under a
* short lock; a background thread (see kl_profiler_start) swaps that buffer out
* and writes the points as JSON lines to `config.filepath`. If the background
* thread is never started, everything still pending is written synchronously
* by kl_profiler_destroy.
**/

#ifndef KL_PROFILER_H
#define KL_PROFILER_H

#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdatomic.h>
#include <pthread.h>
#include <kl/string/string.h>
#include <kl/templates/list/list.h>
#include <kl/threads/mutex.h>
#include <kl/templates/pool/binary_pool.h>

// Labels are copied into the point when a block ends, so the caller's string
// only has to live for the duration of the block. Longer labels are truncated.
#define KL_PROFILER_LABEL_CAP 128

typedef struct {
  double timestamp; // monotonic ms, when the block began
  double duration;  // ms
  int32_t depth;    // nesting depth on the recording thread (0 = outermost)
  uint64_t thread_id;
  char label[KL_PROFILER_LABEL_CAP]; // NUL-terminated
} KL_ProfilerPoint;

typedef enum {
  KL_PROFILER_MESSAGE_NONE,
  KL_PROFILER_MESSAGE_POINT,
} KL_ProfilerMessageType;

typedef struct {
  KL_ProfilerMessageType type;

  union {
    struct { uint8_t unused; } none;
    KL_ProfilerPoint point;
  } as;
} KL_ProfilerMessage;

KL_DEFINE_LIST(profiler_message, KL_ProfilerMessage);

typedef struct KL_Profiler KL_Profiler;

// One open block on a thread's block stack.
typedef struct {
  double timestamp;
  KL_StrConst name;
} KL_ProfilerFrame;

// Per-thread block stack. Lives in a slot of `KL_Profiler.stacks.pool`; slot
// addresses are stable so the pointer can be cached in thread-local storage.
// `frames` is sized to `config.max_block_depth`.
typedef struct {
  KL_Profiler* owner;
  int64_t handle;   // pool handle of this stack
  int64_t depth;    // logical nesting depth (may exceed max_block_depth)
  uint64_t thread_id;
  KL_ProfilerFrame frames[];
} KL_ProfilerBlockStack;

typedef struct {
  // Max number of finished blocks buffered before they are written. When the
  // buffer is full, new points are dropped (see stats.dropped). Default 4096.
  int64_t max_queue_size;

  // Max nesting depth recorded per thread. Deeper blocks are counted so
  // begin/end stay balanced, but produce no point. Default 64.
  int64_t max_block_depth;

  // Number of per-thread stacks allocated at once. More threads than this are
  // fine; the backing pool grows in chunks of this size. Default 16.
  int64_t max_threads;

  // How long the writer thread sleeps when there is nothing to write. Default 1000.
  int64_t poll_interval_us;

  // Output file (JSON lines). Truncated on init. Required.
  KL_StrConst filepath;
} KL_ProfilerConfig;

struct KL_Profiler {
  bool initialized;
  KL_ProfilerConfig config;

  struct {
    FILE* fp;
  } output;

  struct {
    // Finished blocks land in `pending` (under `lock`). The writer swaps
    // `pending` and `draining` so producers only ever wait for a swap.
    KL_profiler_message_List pending;
    KL_profiler_message_List draining;
    KL_Mutex lock;
  } messages;

  struct {
    KL_BinaryPool pool; // of KL_ProfilerBlockStack
    pthread_key_t key;  // -> KL_ProfilerBlockStack* for the calling thread
  } stacks;

  struct {
    pthread_t id;
    atomic_bool running;
  } thread;

  struct {
    atomic_int_least64_t submitted; // points accepted into the pending buffer
    atomic_int_least64_t written;   // points written (and flushed) to the file
    atomic_int_least64_t dropped;   // points discarded because the buffer was full
  } stats;
};

// Opens the output file. Returns false (leaving the profiler uninitialized)
// if it cannot be opened. Calling init on an initialized profiler is a no-op.
bool kl_profiler_init(KL_Profiler* prof, KL_ProfilerConfig cfg);

// Stops the writer thread if running, writes whatever is still pending, closes
// the file and zeroes the struct.
void kl_profiler_destroy(KL_Profiler* prof);

// Starts the background writer thread. Returns true if it is running.
bool kl_profiler_start(KL_Profiler* prof);

// Stops the writer thread. Everything submitted before the call is written
// before this returns.
void kl_profiler_kill(KL_Profiler* prof);

// Blocks until every point submitted before the call has been written to the
// file. Returns false if the writer thread is not running.
bool kl_profiler_flush(KL_Profiler* prof);

// Pushes a block onto the calling thread's stack. Returns false if the block
// will not be recorded (nesting deeper than max_block_depth, or no stack
// could be allocated); end_block must still be called to keep the stack balanced.
bool kl_profiler_begin_block(KL_Profiler* prof, KL_StrConst name);

// Pops the innermost open block on the calling thread and submits its point.
// Returns true if a point was submitted; false if there was no open block, the
// block was too deep to record, or the pending buffer was full.
bool kl_profiler_end_block(KL_Profiler* prof);

// Number of currently open blocks on the calling thread.
int64_t kl_profiler_depth(KL_Profiler* prof);

#endif
