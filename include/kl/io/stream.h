/**
 * KL_IOStream — typed message passing between threads.
 *
 *   Funnel : the "contract" for one `int32_t type`: the size + alignment every
 *            chunk of that type has. Owns the set of sinks for that type.
 *   Sink   : a bounded queue that one reader thread polls. Every write of the
 *            funnel's type is copied into every sink of that funnel.
 *   Chunk  : one message; a copy of the written bytes plus the type.
 *
 * Threading model
 *   - Funnels are created in a setup phase. `kl_io_stream_create_funnel` is
 *     serialised against itself, but the funnel table is read lock-free by the
 *     hot paths, so funnels must NOT be created concurrently with
 *     write/read/create_sink/close_sink on the same stream.
 *   - After setup: any number of writer threads may call `kl_io_stream_write`
 *     and any number of reader threads may call `kl_io_stream_read_chunk` /
 *     `kl_io_stream_chunk_free` / `kl_io_stream_drain_sink` concurrently, and
 *     sinks may be created and closed while writers are active.
 *   - A sink has one reader. Close it from that reader (or after the reader
 *     has stopped); closing a sink while another thread is reading it is a
 *     use-after-free by contract.
 *   - `kl_io_stream_destroy` must run alone.
 *
 * Locking: each write takes the funnel's sink-set lock (so the sink set can't
 * change mid-broadcast) and, per sink, the blob allocator lock + that sink's
 * queue lock. A read takes the pool lock (handle validation) + the queue lock,
 * and freeing a chunk takes the allocator lock. Nothing is held across a
 * user-visible call boundary, and readers never touch the funnel lock, so a
 * reader can't stall the writer beyond one queue push.
 **/
#ifndef KL_IO_STREAM_H
#define KL_IO_STREAM_H

#include <stdbool.h>
#include <stdint.h>

#include <kl/threads/mutex.h>
#include <kl/binary/blob.h>

#include <kl/templates/queue/queue.h>
#include <kl/templates/hashmap/hashmap.h>
#include <kl/templates/pool/binary_pool.h>

#include <kl/memory/allocator.h>


typedef struct {
  KL_Allocation blob_allocation;
  KL_Blob blob;
  int32_t type;
} KL_IOStreamChunk;

KL_DEFINE_QUEUE(io_stream_chunk, KL_IOStreamChunk);
KL_DEFINE_LIST(io_stream_chunk, KL_IOStreamChunk);

typedef struct {
  int64_t max_queue_length;
  int32_t type;
} KL_IOStreamSinkConfig;

typedef struct {
  bool initialized;
  int64_t id;
  KL_IOStreamSinkConfig config;
  KL_io_stream_chunk_Queue queue;
} KL_IOStreamSink;

typedef struct {
  int32_t type;
  struct {
    int64_t size;
    int64_t alignment;
  } chunk;
} KL_IOStreamSinkFunnelConfig;

typedef struct {
  bool initialized;
  KL_IOStreamSinkFunnelConfig config;
  // Slots of KL_IOStreamSink. Slot memory never moves, so a sink pointer is
  // stable until that sink is closed.
  KL_BinaryPool sinks;
  // Guards the *set* of sinks: taken while a write broadcasts and while a
  // sink is created/closed. Readers never take it.
  KL_Mutex lock;
} KL_IOStreamSinkFunnel;

// Funnels are heap-allocated individually and the table stores pointers, so a
// funnel's address (and the mutexes inside it) never move when the table grows.
typedef KL_IOStreamSinkFunnel* KL_IOStreamSinkFunnelPtr;
KL_DEFINE_HASHMAP(io_stream_sink_funnel, KL_IOStreamSinkFunnelPtr);

typedef struct {
  int64_t sink_count_capacity;   // initial sinks per funnel (grows when full)
  int64_t funnel_count_capacity;
  int64_t approx_blob_size;      // initial arena size for chunk payloads (grows when full)
} KL_IOStreamConfig;

typedef struct {
  bool initialized;
  KL_IOStreamConfig config;
  KL_io_stream_sink_funnel_Hashmap funnels; // keyed by int32_t type
  KL_Mutex funnels_lock;                    // serialises create_funnel
  KL_Allocator blob_allocator;
} KL_IOStream;

void kl_io_stream_init(KL_IOStream* stream, KL_IOStreamConfig cfg);

void kl_io_stream_destroy(KL_IOStream* stream);

// Returns 0 if no funnel exists for `type`.
KL_IOStreamSinkFunnel* kl_io_stream_get_funnel(KL_IOStream* stream, int32_t type);

// Idempotent for an identical config; asserts if a funnel for `cfg.type`
// already exists with a different chunk size/alignment. Setup-phase only (see
// threading model above).
KL_IOStreamSinkFunnel* kl_io_stream_create_funnel(KL_IOStream* stream, KL_IOStreamSinkFunnelConfig cfg);

// Returns the sink id (>= 0), or -1 if no funnel exists for `cfg.type`.
int64_t kl_io_stream_create_sink(KL_IOStream* stream, KL_IOStreamSinkConfig cfg);

// Returns 0 if the id is stale/unknown.
KL_IOStreamSink* kl_io_stream_get_sink(KL_IOStream* stream, int32_t type, int64_t id);

// Frees any queued chunks and releases the sink. No-op for a stale id.
void kl_io_stream_close_sink(KL_IOStream* stream, int32_t type, int64_t id);

// Copies `data` into every sink of the funnel for `type`. `data` must match
// the funnel's chunk size + alignment exactly. A sink whose queue is at
// `max_queue_length` drops this chunk (newest-dropped). Returns the number of
// sinks that accepted the chunk; -1 if no funnel exists for `type`.
int64_t kl_io_stream_write(KL_IOStream* stream, int32_t type, KL_BlobAligned data);

// Same as `kl_io_stream_write`, but never blocks indefinitely: the funnel lock
// and each sink's queue lock are attempted with `timeout_ms` (<= 0 → a single
// trylock each). If the funnel lock can't be taken in time nothing is written
// and -2 is returned. If a sink's queue lock can't be taken in time the chunk
// is dropped for that sink only (same as a full queue) and not counted.
// Note: the blob allocator lock is still blocking; it is only ever held for
// the duration of one malloc/free, never across a wait.
int64_t kl_io_stream_write_timeout(KL_IOStream* stream, int32_t type, KL_BlobAligned data, double timeout_ms);

// Pops the oldest chunk from a sink. Returns false if the sink is unknown, the
// queue is empty, or the queue lock could not be taken within `timeout_ms`
// (<= 0 → a single trylock). NOTE: the timeout bounds the wait for the *lock*,
// not for data — this is a poll, it never waits for a chunk to arrive.
// Caller must call `kl_io_stream_chunk_free` on `out` after use.
bool kl_io_stream_read_chunk(KL_IOStream* stream, int32_t type, int64_t sink_id, double timeout_ms, KL_IOStreamChunk* out);

bool kl_io_stream_read_chunks(KL_IOStream* stream, int32_t type, int64_t sink_id, double timeout_ms, int64_t limit, KL_io_stream_chunk_List* out);

void kl_io_stream_chunk_free(KL_IOStream* stream, KL_IOStreamChunk* chunk);

// Discards (and frees) every queued chunk in a sink.
void kl_io_stream_drain_sink(KL_IOStream* stream, int32_t type, int64_t sink_id);

// Number of chunks currently queued in a sink; -1 if the sink is unknown.
int64_t kl_io_stream_sink_length(KL_IOStream* stream, int32_t type, int64_t sink_id);
#endif
