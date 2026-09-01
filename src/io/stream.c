#include <kl/io/stream.h>
#include <kl/macros.h>
#include <kl/memory/alloc.h>
#include <assert.h>
#include <stdalign.h>
#include <stdlib.h>
#include <string.h>

KL_IMPL_QUEUE(io_stream_chunk, KL_IOStreamChunk);
KL_IMPL_LIST(io_stream_chunk, KL_IOStreamChunk);
KL_IMPL_HASHMAP(io_stream_sink_funnel, KL_IOStreamSinkFunnelPtr);


void kl_io_stream_init(KL_IOStream* stream, KL_IOStreamConfig cfg) {
  if (stream->initialized) return;
  assert(cfg.funnel_count_capacity > 0);
  assert(cfg.sink_count_capacity > 0);
  assert(cfg.approx_blob_size > 0);

  stream->config = cfg;

  kl_allocator_init(&stream->blob_allocator, (KL_AllocatorConfig){
    .capacity = cfg.approx_blob_size,
    .tracked_allocations_capacity = KL_MAX(cfg.sink_count_capacity * 16, 64),
    .track_allocations = true
  });

  kl_io_stream_sink_funnel_hashmap_init(&stream->funnels, (KL_HashmapConfig){
    .capacity = KL_MAX(cfg.funnel_count_capacity, 64) * 2 + 7,
    .collision_capacity = 4,
    .track_used_slots = true
  });

  kl_mutex_init(&stream->funnels_lock);

  stream->initialized = true;
}

// Frees every chunk still queued in `sink`. Caller holds sink->queue.lock (or
// is otherwise the sole owner of the sink).
static void kl_io_stream_free_queued_chunks_(KL_IOStream* stream, KL_IOStreamSink* sink) {
  for (int64_t i = 0; i < sink->queue.length; i++) {
    KL_IOStreamChunk* chunk = &sink->queue.items[i];
    kl_allocator_free_allocation(&stream->blob_allocator, chunk->blob_allocation);
    memset(chunk, 0, sizeof(KL_IOStreamChunk));
  }
  kl_io_stream_chunk_queue_reset(&sink->queue);
}

static void kl_io_stream_funnel_destroy_(KL_IOStream* stream, KL_IOStreamSinkFunnel* funnel) {
  assert(funnel->initialized);

  for (int64_t j = 0; j < funnel->sinks.taken_indices.length; j++) {
    int64_t idx = funnel->sinks.taken_indices.items[j];
    KL_IOStreamSink* sink = (KL_IOStreamSink*)kl_binary_pool_get_slot(&funnel->sinks, idx);
    if (!sink->initialized) continue;
    // The blob allocator is torn down right after, so no need to free chunks.
    kl_io_stream_chunk_queue_destroy(&sink->queue);
  }

  kl_binary_pool_destroy(&funnel->sinks);
  kl_mutex_destroy(&funnel->lock);
  memset(funnel, 0, sizeof(KL_IOStreamSinkFunnel));
  free(funnel);
}

void kl_io_stream_destroy(KL_IOStream* stream) {
  assert(stream->initialized);

  for (int64_t i = 0; i < stream->funnels.used_slots.length; i++) {
    KL_io_stream_sink_funnel_HashmapSlot* slot = &stream->funnels.slots.items[stream->funnels.used_slots.items[i]];
    if (!slot->is_set) continue;
    if (slot->value == 0) continue;
    kl_io_stream_funnel_destroy_(stream, slot->value);
    slot->value = 0;
  }

  kl_io_stream_sink_funnel_hashmap_destroy(&stream->funnels);
  kl_mutex_destroy(&stream->funnels_lock);
  kl_allocator_destroy(&stream->blob_allocator);

  memset(stream, 0, sizeof(KL_IOStream));
}

KL_IOStreamSinkFunnel* kl_io_stream_get_funnel(KL_IOStream* stream, int32_t type) {
  assert(stream->initialized);
  KL_Blob key = KL_BLOB_RAW_T(int32_t, type);

  KL_IOStreamSinkFunnelPtr funnel = 0;
  if (!kl_io_stream_sink_funnel_hashmap_get(&stream->funnels, key, &funnel)) return 0;

  assert(funnel != 0);
  assert(funnel->initialized);

  return funnel;
}

KL_IOStreamSinkFunnel* kl_io_stream_create_funnel(KL_IOStream* stream, KL_IOStreamSinkFunnelConfig cfg) {
  assert(stream->initialized);
  assert(cfg.chunk.size > 0);
  assert(cfg.chunk.alignment > 0);

  KL_Blob key = KL_BLOB_RAW_T(int32_t, cfg.type);

  kl_mutex_lock(&stream->funnels_lock);

  KL_IOStreamSinkFunnelPtr old = 0;
  if (kl_io_stream_sink_funnel_hashmap_get(&stream->funnels, key, &old)) {
    assert(old != 0);
    assert(old->initialized);
    assert(old->config.chunk.size == cfg.chunk.size);
    assert(old->config.chunk.alignment == cfg.chunk.alignment);
    kl_mutex_unlock(&stream->funnels_lock);
    return old;
  }

  // Heap-allocated on its own so the address is stable for the stream's
  // lifetime: the funnel owns mutexes, which must never be moved, and the
  // hashmap may realloc its slot array when it grows.
  KL_IOStreamSinkFunnel* funnel = (KL_IOStreamSinkFunnel*)kl_aligned_alloc(
    sizeof(KL_IOStreamSinkFunnel), alignof(KL_IOStreamSinkFunnel));
  assert(funnel != 0);
  memset(funnel, 0, sizeof(KL_IOStreamSinkFunnel));

  kl_binary_pool_init(&funnel->sinks, (KL_BinaryPoolConfig){
    .count = stream->config.sink_count_capacity,
    .element_size = sizeof(KL_IOStreamSink),
    .element_alignment = alignof(KL_IOStreamSink),
    .grow_when_full = true
  });

  kl_mutex_init(&funnel->lock);
  funnel->config = cfg;
  funnel->initialized = true;

  kl_io_stream_sink_funnel_hashmap_set(&stream->funnels, key, funnel);

  kl_mutex_unlock(&stream->funnels_lock);

  return funnel;
}


int64_t kl_io_stream_create_sink(KL_IOStream* stream, KL_IOStreamSinkConfig cfg) {
  assert(stream->initialized);
  assert(cfg.max_queue_length > 0);

  KL_IOStreamSinkFunnel* funnel = kl_io_stream_get_funnel(stream, cfg.type);
  if (funnel == 0) return -1;

  kl_mutex_lock(&funnel->lock);

  uint8_t* raw = 0;
  int64_t id = kl_binary_pool_acquire(&funnel->sinks, &raw);
  assert(raw != 0);
  assert(id >= 0);

  // Slot memory is zeroed by the pool on first use and by close_sink on
  // release, so the queue's `initialized` flag is false here.
  KL_IOStreamSink* sink = (KL_IOStreamSink*)raw;
  assert(!sink->initialized);
  sink->config = cfg;
  sink->id = id;
  kl_io_stream_chunk_queue_init(&sink->queue, (KL_QueueConfig){
    .capacity = cfg.max_queue_length,
    .max_length = cfg.max_queue_length,
  });
  sink->initialized = true;

  kl_mutex_unlock(&funnel->lock);

  return id;
}


KL_IOStreamSink* kl_io_stream_get_sink(KL_IOStream* stream, int32_t type, int64_t id) {
  assert(stream->initialized);
  if (id < 0) return 0;
  KL_IOStreamSinkFunnel* funnel = kl_io_stream_get_funnel(stream, type);
  if (funnel == 0) return 0;

  return (KL_IOStreamSink*)kl_binary_pool_get(&funnel->sinks, id);
}


void kl_io_stream_close_sink(KL_IOStream* stream, int32_t type, int64_t id) {
  assert(stream->initialized);
  KL_IOStreamSinkFunnel* funnel = kl_io_stream_get_funnel(stream, type);
  if (funnel == 0) return;

  kl_mutex_lock(&funnel->lock);

  KL_IOStreamSink* sink = (KL_IOStreamSink*)kl_binary_pool_get(&funnel->sinks, id);
  if (sink == 0) {
    kl_mutex_unlock(&funnel->lock);
    return;
  }
  assert(sink->initialized);

  // Holding funnel->lock excludes writers, so nothing can push while we tear
  // the queue down. Return the queued payloads to the allocator first —
  // otherwise they'd stay allocated until the stream is destroyed.
  kl_mutex_lock(&sink->queue.lock);
  kl_io_stream_free_queued_chunks_(stream, sink);
  kl_mutex_unlock(&sink->queue.lock);

  kl_io_stream_chunk_queue_destroy(&sink->queue);
  memset(sink, 0, sizeof(KL_IOStreamSink));

  kl_binary_pool_release(&funnel->sinks, id);

  kl_mutex_unlock(&funnel->lock);
}

int64_t kl_io_stream_write(KL_IOStream* stream, int32_t type, KL_BlobAligned data) {
  assert(stream->initialized);
  KL_IOStreamSinkFunnel* funnel = kl_io_stream_get_funnel(stream, type);
  if (funnel == 0) return -1;

  assert(data.value != 0);
  assert(data.size == funnel->config.chunk.size);
  assert(data.alignment == funnel->config.chunk.alignment);

  int64_t accepted = 0;

  // The sink set must not change while we iterate it: create/close both take
  // this lock. Readers don't, so they keep popping while we broadcast.
  kl_mutex_lock(&funnel->lock);

  for (int64_t i = 0; i < funnel->sinks.taken_indices.length; i++) {
    int64_t idx = funnel->sinks.taken_indices.items[i];
    KL_IOStreamSink* sink = (KL_IOStreamSink*)kl_binary_pool_get_slot(&funnel->sinks, idx);
    assert(sink != 0);
    assert(sink->initialized);
    if (sink->queue.length >= sink->config.max_queue_length) continue;

    // Each sink gets its own copy: the consumer of a chunk owns and frees it.
    KL_IOStreamChunk chunk = {0};
    chunk.type = type;
    chunk.blob.size = data.size;
    chunk.blob.value = kl_allocator_malloc_tracked_aligned(
      &stream->blob_allocator, data.size, (size_t)data.alignment, &chunk.blob_allocation);
    assert(chunk.blob.value != 0);
    memcpy(chunk.blob.value, data.value, (size_t)data.size);

    if (kl_io_stream_chunk_queue_push(&sink->queue, chunk)) {
      accepted += 1;
    } else {
      // Queue is at max_length: drop this chunk for this sink.
      kl_allocator_free_allocation(&stream->blob_allocator, chunk.blob_allocation);
    }
  }

  kl_mutex_unlock(&funnel->lock);

  return accepted;
}


bool kl_io_stream_read_chunk(KL_IOStream* stream, int32_t type, int64_t sink_id, double timeout_ms, KL_IOStreamChunk* out) {
  assert(stream->initialized);
  assert(out != 0);
  KL_IOStreamSink* sink = kl_io_stream_get_sink(stream, type, sink_id);
  if (sink == 0) return false;
  assert(sink->initialized);
  return kl_io_stream_chunk_queue_try_pop_first_timeout(&sink->queue, out, timeout_ms);
}

bool kl_io_stream_read_chunks(KL_IOStream* stream, int32_t type, int64_t sink_id, double timeout_ms, int64_t limit, KL_io_stream_chunk_List* out) {
  assert(stream->initialized);
  assert(out != 0);
  assert(out->initialized);

  bool did_read = false;

  for (int64_t i = 0; i < limit; i++) {
    KL_IOStreamChunk chunk = {0};
    if (!kl_io_stream_read_chunk(stream, type, sink_id, timeout_ms, &chunk)) {
      // Sink is empty (or unknown / lock timed out): nothing more to take.
      break;
    }
    kl_io_stream_chunk_list_push(out, chunk);
    did_read = true;
  }

  return did_read;
}

void kl_io_stream_chunk_free(KL_IOStream* stream, KL_IOStreamChunk* chunk) {
  assert(stream->initialized);
  assert(chunk != 0);
  if (chunk->blob.value == 0) return;
  kl_allocator_free_allocation(&stream->blob_allocator, chunk->blob_allocation);
  memset(chunk, 0, sizeof(KL_IOStreamChunk));
}

void kl_io_stream_drain_sink(KL_IOStream* stream, int32_t type, int64_t sink_id) {
  assert(stream->initialized);
  KL_IOStreamSink* sink = kl_io_stream_get_sink(stream, type, sink_id);
  if (sink == 0) return;

  kl_mutex_lock(&sink->queue.lock);
  kl_io_stream_free_queued_chunks_(stream, sink);
  kl_mutex_unlock(&sink->queue.lock);
}

int64_t kl_io_stream_sink_length(KL_IOStream* stream, int32_t type, int64_t sink_id) {
  assert(stream->initialized);
  KL_IOStreamSink* sink = kl_io_stream_get_sink(stream, type, sink_id);
  if (sink == 0) return -1;

  kl_mutex_lock(&sink->queue.lock);
  int64_t length = sink->queue.length;
  kl_mutex_unlock(&sink->queue.lock);
  return length;
}
