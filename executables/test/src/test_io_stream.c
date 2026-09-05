#include <test/tests.h>
#include <pthread.h>
#include <sched.h>
#include <stdalign.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <kl/time/time.h>

// ---------- io_stream ----------

enum {
  KL_TEST_IOS_TYPE_A = 1,
  KL_TEST_IOS_TYPE_B = 2,
  KL_TEST_IOS_TYPE_UNKNOWN = 99,
};

typedef struct {
  int64_t seq;
  int32_t tag;
} KL_TestIOSMsgA;

typedef struct {
  _Alignas(32) double v[4];
} KL_TestIOSMsgB;

static void kl_test_ios_setup(KL_IOStream* s, int64_t sink_capacity) {
  *s = (KL_IOStream){0};
  kl_io_stream_init(s, (KL_IOStreamConfig){
    .sink_count_capacity = sink_capacity,
    .funnel_count_capacity = 8,
    .approx_blob_size = 4096,
  });
  kl_io_stream_create_funnel(s, (KL_IOStreamSinkFunnelConfig){
    .type = KL_TEST_IOS_TYPE_A,
    .chunk = { .size = sizeof(KL_TestIOSMsgA), .alignment = alignof(KL_TestIOSMsgA) },
  });
  kl_io_stream_create_funnel(s, (KL_IOStreamSinkFunnelConfig){
    .type = KL_TEST_IOS_TYPE_B,
    .chunk = { .size = sizeof(KL_TestIOSMsgB), .alignment = alignof(KL_TestIOSMsgB) },
  });
}

// Number of chunk payloads currently handed out by the blob allocator
// (tracked slots minus the ones sitting on its free lists).
static int64_t kl_test_ios_live_blobs(KL_IOStream* s) {
  return s->blob_allocator.allocations.length - s->blob_allocator.allocations.n_freed;
}

static int64_t kl_test_ios_write_a(KL_IOStream* s, int64_t seq, int32_t tag) {
  KL_TestIOSMsgA msg = { .seq = seq, .tag = tag };
  return kl_io_stream_write(s, KL_TEST_IOS_TYPE_A, KL_BLOB_ALIGNED_RAW_T(KL_TestIOSMsgA, msg));
}

void test_io_stream_init_destroy() {
  KL_IOStream s = {0};
  kl_io_stream_init(&s, (KL_IOStreamConfig){
    .sink_count_capacity = 4, .funnel_count_capacity = 4, .approx_blob_size = 1024,
  });
  KL_TEST_EXPECT_EXPR(s.initialized);
  KL_TEST_EXPECT_EXPR(s.funnels.initialized);
  KL_TEST_EXPECT_EXPR(s.blob_allocator.initialized);
  KL_TEST_EXPECT_EXPR(s.blob_allocator.config.track_allocations);

  // double init is a no-op
  kl_io_stream_init(&s, (KL_IOStreamConfig){
    .sink_count_capacity = 99, .funnel_count_capacity = 99, .approx_blob_size = 99,
  });
  KL_TEST_EXPECT_EQ_INT(s.config.sink_count_capacity, 4);

  kl_io_stream_destroy(&s);
  KL_TEST_EXPECT_EXPR(!s.initialized);
}

void test_io_stream_create_funnel_is_idempotent() {
  KL_IOStream s;
  kl_test_ios_setup(&s, 4);

  KL_IOStreamSinkFunnel* f1 = kl_io_stream_get_funnel(&s, KL_TEST_IOS_TYPE_A);
  KL_TEST_EXPECT_EXPR(f1 != 0);
  KL_TEST_EXPECT_EXPR(f1->initialized);
  KL_TEST_EXPECT_EQ_INT(f1->config.type, KL_TEST_IOS_TYPE_A);
  KL_TEST_EXPECT_EQ_INT(f1->config.chunk.size, (int64_t)sizeof(KL_TestIOSMsgA));

  KL_IOStreamSinkFunnel* f2 = kl_io_stream_create_funnel(&s, (KL_IOStreamSinkFunnelConfig){
    .type = KL_TEST_IOS_TYPE_A,
    .chunk = { .size = sizeof(KL_TestIOSMsgA), .alignment = alignof(KL_TestIOSMsgA) },
  });
  KL_TEST_ASSERT(f1 == f2, "re-creating a funnel returns the existing one");

  KL_TEST_EXPECT_EXPR(kl_io_stream_get_funnel(&s, KL_TEST_IOS_TYPE_UNKNOWN) == 0);

  kl_io_stream_destroy(&s);
}

void test_io_stream_funnel_pointers_stable_across_many_creates() {
  KL_IOStream s = {0};
  kl_io_stream_init(&s, (KL_IOStreamConfig){
    .sink_count_capacity = 2, .funnel_count_capacity = 1, .approx_blob_size = 1024,
  });

  enum { N = 2000 };
  KL_IOStreamSinkFunnel* ptrs[N];
  for (int32_t i = 0; i < N; i++) {
    ptrs[i] = kl_io_stream_create_funnel(&s, (KL_IOStreamSinkFunnelConfig){
      .type = i * 7919, // spread keys
      .chunk = { .size = 8, .alignment = 8 },
    });
    KL_TEST_EXPECT_EXPR(ptrs[i] != 0);
  }

  // The table has grown well past its initial capacity; every funnel pointer
  // handed out earlier must still resolve to the same live object.
  for (int32_t i = 0; i < N; i++) {
    KL_IOStreamSinkFunnel* f = kl_io_stream_get_funnel(&s, i * 7919);
    KL_TEST_ASSERT(f == ptrs[i], "funnel pointer must not move when the table grows");
    KL_TEST_EXPECT_EXPR(f->initialized);
    KL_TEST_EXPECT_EQ_INT(f->config.type, i * 7919);
  }

  kl_io_stream_destroy(&s);
}

void test_io_stream_create_sink_without_funnel_fails() {
  KL_IOStream s;
  kl_test_ios_setup(&s, 4);

  int64_t id = kl_io_stream_create_sink(&s, (KL_IOStreamSinkConfig){
    .type = KL_TEST_IOS_TYPE_UNKNOWN, .max_queue_length = 4,
  });
  KL_TEST_EXPECT_EQ_INT(id, -1);
  KL_TEST_EXPECT_EXPR(kl_io_stream_get_sink(&s, 0) == 0);
  KL_TEST_EXPECT_EXPR(kl_io_stream_get_sink(&s, -1) == 0);
  KL_TEST_EXPECT_EXPR(kl_io_stream_get_sink(&s, 12345) == 0);

  kl_io_stream_destroy(&s);
}

void test_io_stream_write_without_sinks() {
  KL_IOStream s;
  kl_test_ios_setup(&s, 4);

  KL_TEST_EXPECT_EQ_INT(kl_test_ios_write_a(&s, 1, 0), 0);

  KL_TestIOSMsgA msg = {0};
  int64_t r = kl_io_stream_write(&s, KL_TEST_IOS_TYPE_UNKNOWN, KL_BLOB_ALIGNED_RAW_T(KL_TestIOSMsgA, msg));
  KL_TEST_EXPECT_EQ_INT(r, -1);

  kl_io_stream_destroy(&s);
}

void test_io_stream_write_read_round_trip() {
  KL_IOStream s;
  kl_test_ios_setup(&s, 4);

  int64_t sink = kl_io_stream_create_sink(&s, (KL_IOStreamSinkConfig){
    .type = KL_TEST_IOS_TYPE_A, .max_queue_length = 8,
  });
  KL_TEST_EXPECT_EXPR(sink >= 0);
  KL_TEST_EXPECT_EXPR(kl_io_stream_get_sink(&s, sink) != 0);

  KL_IOStreamChunk chunk = {0};
  KL_TEST_ASSERT(!kl_io_stream_read_chunk(&s, sink, 0, &chunk), "empty sink reads nothing");

  KL_TEST_EXPECT_EQ_INT(kl_test_ios_write_a(&s, 42, 7), 1);
  KL_TEST_EXPECT_EQ_INT(kl_io_stream_sink_length(&s, sink), 1);

  KL_TEST_ASSERT(kl_io_stream_read_chunk(&s, sink, 0, &chunk), "chunk available");
  KL_TEST_EXPECT_EQ_INT(chunk.type, KL_TEST_IOS_TYPE_A);
  KL_TEST_EXPECT_EQ_INT(chunk.blob.size, (int64_t)sizeof(KL_TestIOSMsgA));
  KL_TEST_EXPECT_EXPR(chunk.blob.value != 0);
  KL_TEST_EXPECT_EQ_INT(((uintptr_t)chunk.blob.value) % alignof(KL_TestIOSMsgA), 0);

  KL_TestIOSMsgA* got = (KL_TestIOSMsgA*)chunk.blob.value;
  KL_TEST_EXPECT_EQ_INT(got->seq, 42);
  KL_TEST_EXPECT_EQ_INT(got->tag, 7);

  kl_io_stream_chunk_free(&s, &chunk);
  KL_TEST_EXPECT_EXPR(chunk.blob.value == 0);
  // freeing an already-freed / zero chunk is harmless
  kl_io_stream_chunk_free(&s, &chunk);

  KL_TEST_EXPECT_EQ_INT(kl_io_stream_sink_length(&s, sink), 0);
  KL_TEST_ASSERT(!kl_io_stream_read_chunk(&s, sink, 0, &chunk), "sink empty again");

  kl_io_stream_destroy(&s);
}

void test_io_stream_chunk_copy_is_independent_of_source() {
  KL_IOStream s;
  kl_test_ios_setup(&s, 4);
  int64_t sink = kl_io_stream_create_sink(&s, (KL_IOStreamSinkConfig){
    .type = KL_TEST_IOS_TYPE_A, .max_queue_length = 8,
  });

  KL_TestIOSMsgA msg = { .seq = 1, .tag = 1 };
  kl_io_stream_write(&s, KL_TEST_IOS_TYPE_A, KL_BLOB_ALIGNED_RAW_T(KL_TestIOSMsgA, msg));
  msg.seq = 999; // mutate the source after writing

  KL_IOStreamChunk chunk = {0};
  KL_TEST_EXPECT_EXPR(kl_io_stream_read_chunk(&s, sink, 0, &chunk));
  KL_TEST_EXPECT_EQ_INT(((KL_TestIOSMsgA*)chunk.blob.value)->seq, 1);
  kl_io_stream_chunk_free(&s, &chunk);

  kl_io_stream_destroy(&s);
}

void test_io_stream_read_preserves_fifo_order() {
  KL_IOStream s;
  kl_test_ios_setup(&s, 4);
  int64_t sink = kl_io_stream_create_sink(&s, (KL_IOStreamSinkConfig){
    .type = KL_TEST_IOS_TYPE_A, .max_queue_length = 64,
  });

  for (int64_t i = 0; i < 50; i++) KL_TEST_EXPECT_EQ_INT(kl_test_ios_write_a(&s, i, 0), 1);
  KL_TEST_EXPECT_EQ_INT(kl_io_stream_sink_length(&s, sink), 50);

  for (int64_t i = 0; i < 50; i++) {
    KL_IOStreamChunk chunk = {0};
    KL_TEST_EXPECT_EXPR(kl_io_stream_read_chunk(&s, sink, 0, &chunk));
    KL_TEST_EXPECT_EQ_INT(((KL_TestIOSMsgA*)chunk.blob.value)->seq, i);
    kl_io_stream_chunk_free(&s, &chunk);
  }

  kl_io_stream_destroy(&s);
}

void test_io_stream_write_broadcasts_to_all_sinks() {
  KL_IOStream s;
  kl_test_ios_setup(&s, 4);

  enum { N = 5 };
  int64_t sinks[N];
  for (int i = 0; i < N; i++) {
    sinks[i] = kl_io_stream_create_sink(&s, (KL_IOStreamSinkConfig){
      .type = KL_TEST_IOS_TYPE_A, .max_queue_length = 8,
    });
    KL_TEST_EXPECT_EXPR(sinks[i] >= 0);
    for (int j = 0; j < i; j++) KL_TEST_EXPECT_EXPR(sinks[i] != sinks[j]);
  }

  KL_TEST_EXPECT_EQ_INT(kl_test_ios_write_a(&s, 11, 0), N);

  for (int i = 0; i < N; i++) {
    KL_IOStreamChunk chunk = {0};
    KL_TEST_EXPECT_EXPR(kl_io_stream_read_chunk(&s, sinks[i], 0, &chunk));
    KL_TEST_EXPECT_EQ_INT(((KL_TestIOSMsgA*)chunk.blob.value)->seq, 11);
    kl_io_stream_chunk_free(&s, &chunk);
    KL_TEST_EXPECT_EXPR(!kl_io_stream_read_chunk(&s, sinks[i], 0, &chunk));
  }

  kl_io_stream_destroy(&s);
}

void test_io_stream_sinks_only_receive_their_type() {
  KL_IOStream s;
  kl_test_ios_setup(&s, 4);

  int64_t sink_a = kl_io_stream_create_sink(&s, (KL_IOStreamSinkConfig){
    .type = KL_TEST_IOS_TYPE_A, .max_queue_length = 8,
  });
  int64_t sink_b = kl_io_stream_create_sink(&s, (KL_IOStreamSinkConfig){
    .type = KL_TEST_IOS_TYPE_B, .max_queue_length = 8,
  });

  KL_TestIOSMsgB b = { .v = { 1.0, 2.0, 3.0, 4.0 } };
  KL_TEST_EXPECT_EQ_INT(kl_io_stream_write(&s, KL_TEST_IOS_TYPE_B, KL_BLOB_ALIGNED_RAW_T(KL_TestIOSMsgB, b)), 1);
  KL_TEST_EXPECT_EQ_INT(kl_test_ios_write_a(&s, 5, 0), 1);

  KL_TEST_EXPECT_EQ_INT(kl_io_stream_sink_length(&s, sink_a), 1);
  KL_TEST_EXPECT_EQ_INT(kl_io_stream_sink_length(&s, sink_b), 1);

  KL_IOStreamChunk chunk = {0};
  KL_TEST_EXPECT_EXPR(kl_io_stream_read_chunk(&s, sink_b, 0, &chunk));
  KL_TEST_EXPECT_EQ_INT(chunk.type, KL_TEST_IOS_TYPE_B);
  KL_TEST_EXPECT_EQ_INT(chunk.blob.size, (int64_t)sizeof(KL_TestIOSMsgB));
  KL_TEST_EXPECT_EQ_INT(((uintptr_t)chunk.blob.value) % 32, 0);
  KL_TEST_EXPECT_EQ_FLOAT(((KL_TestIOSMsgB*)chunk.blob.value)->v[3], 4.0, 0.0);
  kl_io_stream_chunk_free(&s, &chunk);

  KL_TEST_EXPECT_EXPR(kl_io_stream_read_chunk(&s, sink_a, 0, &chunk));
  KL_TEST_EXPECT_EQ_INT(chunk.type, KL_TEST_IOS_TYPE_A);
  kl_io_stream_chunk_free(&s, &chunk);

  // A sink id is only meaningful within its own funnel.
  KL_TEST_EXPECT_EXPR(!kl_io_stream_read_chunk(&s, sink_b, 0, &chunk));

  kl_io_stream_destroy(&s);
}

void test_io_stream_full_sink_drops_newest() {
  KL_IOStream s;
  kl_test_ios_setup(&s, 4);
  int64_t sink = kl_io_stream_create_sink(&s, (KL_IOStreamSinkConfig){
    .type = KL_TEST_IOS_TYPE_A, .max_queue_length = 3,
  });

  KL_TEST_EXPECT_EQ_INT(kl_test_ios_write_a(&s, 0, 0), 1);
  KL_TEST_EXPECT_EQ_INT(kl_test_ios_write_a(&s, 1, 0), 1);
  KL_TEST_EXPECT_EQ_INT(kl_test_ios_write_a(&s, 2, 0), 1);
  KL_TEST_EXPECT_EQ_INT(kl_test_ios_live_blobs(&s), 3);
  KL_TEST_EXPECT_EQ_INT(kl_test_ios_write_a(&s, 3, 0), 0); // dropped
  KL_TEST_EXPECT_EQ_INT(kl_test_ios_write_a(&s, 4, 0), 0); // dropped
  // the dropped copies were returned to the allocator, not leaked
  KL_TEST_EXPECT_EQ_INT(kl_test_ios_live_blobs(&s), 3);
  KL_TEST_EXPECT_EQ_INT(kl_io_stream_sink_length(&s, sink), 3);

  for (int64_t i = 0; i < 3; i++) {
    KL_IOStreamChunk chunk = {0};
    KL_TEST_EXPECT_EXPR(kl_io_stream_read_chunk(&s, sink, 0, &chunk));
    KL_TEST_EXPECT_EQ_INT(((KL_TestIOSMsgA*)chunk.blob.value)->seq, i);
    kl_io_stream_chunk_free(&s, &chunk);
  }

  // space again after reading
  KL_TEST_EXPECT_EQ_INT(kl_test_ios_write_a(&s, 5, 0), 1);

  kl_io_stream_destroy(&s);
}

void test_io_stream_drain_sink() {
  KL_IOStream s;
  kl_test_ios_setup(&s, 4);
  int64_t sink = kl_io_stream_create_sink(&s, (KL_IOStreamSinkConfig){
    .type = KL_TEST_IOS_TYPE_A, .max_queue_length = 8,
  });

  for (int64_t i = 0; i < 5; i++) kl_test_ios_write_a(&s, i, 0);
  KL_TEST_EXPECT_EQ_INT(kl_test_ios_live_blobs(&s), 5);

  kl_io_stream_drain_sink(&s, sink);
  KL_TEST_EXPECT_EQ_INT(kl_io_stream_sink_length(&s, sink), 0);
  KL_TEST_EXPECT_EQ_INT(kl_test_ios_live_blobs(&s), 0);

  KL_IOStreamChunk chunk = {0};
  KL_TEST_EXPECT_EXPR(!kl_io_stream_read_chunk(&s, sink, 0, &chunk));

  // still usable afterwards
  KL_TEST_EXPECT_EQ_INT(kl_test_ios_write_a(&s, 9, 0), 1);
  KL_TEST_EXPECT_EXPR(kl_io_stream_read_chunk(&s, sink, 0, &chunk));
  KL_TEST_EXPECT_EQ_INT(((KL_TestIOSMsgA*)chunk.blob.value)->seq, 9);
  kl_io_stream_chunk_free(&s, &chunk);

  // draining unknown sinks is a no-op
  kl_io_stream_drain_sink(&s, 777);

  kl_io_stream_destroy(&s);
}

void test_io_stream_close_sink() {
  KL_IOStream s;
  kl_test_ios_setup(&s, 4);

  int64_t a = kl_io_stream_create_sink(&s, (KL_IOStreamSinkConfig){
    .type = KL_TEST_IOS_TYPE_A, .max_queue_length = 8,
  });
  int64_t b = kl_io_stream_create_sink(&s, (KL_IOStreamSinkConfig){
    .type = KL_TEST_IOS_TYPE_A, .max_queue_length = 8,
  });

  KL_TEST_EXPECT_EQ_INT(kl_test_ios_write_a(&s, 1, 0), 2);
  KL_TEST_EXPECT_EQ_INT(kl_test_ios_write_a(&s, 2, 0), 2);

  KL_TEST_EXPECT_EQ_INT(kl_test_ios_live_blobs(&s), 4);
  int64_t a_pool_handle = kl_io_stream_get_sink(&s, a)->pool_handle;
  kl_io_stream_close_sink(&s, a);
  // queued chunks of the closed sink were released
  KL_TEST_EXPECT_EQ_INT(kl_test_ios_live_blobs(&s), 2);

  KL_TEST_EXPECT_EXPR(kl_io_stream_get_sink(&s, a) == 0);
  KL_TEST_EXPECT_EQ_INT(kl_io_stream_sink_length(&s, a), -1);
  KL_IOStreamChunk chunk = {0};
  KL_TEST_EXPECT_EXPR(!kl_io_stream_read_chunk(&s, a, 0, &chunk));

  // closing again / closing unknown ids is a no-op
  kl_io_stream_close_sink(&s, a);
  kl_io_stream_close_sink(&s, 4242);

  // writes now only reach b
  KL_TEST_EXPECT_EQ_INT(kl_test_ios_write_a(&s, 3, 0), 1);
  KL_TEST_EXPECT_EQ_INT(kl_io_stream_sink_length(&s, b), 3);

  // a new sink reuses the freed slot but gets a fresh id; the stale id stays dead
  int64_t c = kl_io_stream_create_sink(&s, (KL_IOStreamSinkConfig){
    .type = KL_TEST_IOS_TYPE_A, .max_queue_length = 8,
  });
  KL_TEST_EXPECT_EXPR(c >= 0);
  KL_TEST_EXPECT_EXPR(c != a);
  KL_TEST_EXPECT_EQ_INT(kl_binary_pool_handle_index(kl_io_stream_get_sink(&s, c)->pool_handle),
                        kl_binary_pool_handle_index(a_pool_handle));
  KL_TEST_EXPECT_EXPR(kl_io_stream_get_sink(&s, a) == 0);
  KL_TEST_EXPECT_EQ_INT(kl_io_stream_sink_length(&s, c), 0);
  KL_TEST_EXPECT_EQ_INT(kl_test_ios_write_a(&s, 4, 0), 2);

  kl_io_stream_destroy(&s);
}

void test_io_stream_sink_pool_grows_when_full() {
  KL_IOStream s;
  kl_test_ios_setup(&s, 2);

  enum { N = 17 };
  int64_t sinks[N];
  for (int i = 0; i < N; i++) {
    sinks[i] = kl_io_stream_create_sink(&s, (KL_IOStreamSinkConfig){
      .type = KL_TEST_IOS_TYPE_A, .max_queue_length = 4,
    });
    KL_TEST_EXPECT_EXPR(sinks[i] >= 0);
  }
  KL_TEST_EXPECT_EQ_INT(kl_test_ios_write_a(&s, 1, 0), N);
  for (int i = 0; i < N; i++) {
    KL_IOStreamChunk chunk = {0};
    KL_TEST_EXPECT_EXPR(kl_io_stream_read_chunk(&s, sinks[i], 0, &chunk));
    kl_io_stream_chunk_free(&s, &chunk);
  }

  kl_io_stream_destroy(&s);
}

void test_io_stream_blob_arena_grows_beyond_approx_size() {
  KL_IOStream s = {0};
  kl_io_stream_init(&s, (KL_IOStreamConfig){
    .sink_count_capacity = 2, .funnel_count_capacity = 2, .approx_blob_size = 64,
  });
  kl_io_stream_create_funnel(&s, (KL_IOStreamSinkFunnelConfig){
    .type = KL_TEST_IOS_TYPE_B,
    .chunk = { .size = sizeof(KL_TestIOSMsgB), .alignment = alignof(KL_TestIOSMsgB) },
  });
  int64_t sink = kl_io_stream_create_sink(&s, (KL_IOStreamSinkConfig){
    .type = KL_TEST_IOS_TYPE_B, .max_queue_length = 1000,
  });

  for (int i = 0; i < 500; i++) {
    KL_TestIOSMsgB b = { .v = { (double)i, 0, 0, 0 } };
    KL_TEST_EXPECT_EQ_INT(kl_io_stream_write(&s, KL_TEST_IOS_TYPE_B, KL_BLOB_ALIGNED_RAW_T(KL_TestIOSMsgB, b)), 1);
  }
  for (int i = 0; i < 500; i++) {
    KL_IOStreamChunk chunk = {0};
    KL_TEST_EXPECT_EXPR(kl_io_stream_read_chunk(&s, sink, 0, &chunk));
    KL_TEST_EXPECT_EQ_INT(((uintptr_t)chunk.blob.value) % 32, 0);
    KL_TEST_EXPECT_EQ_FLOAT(((KL_TestIOSMsgB*)chunk.blob.value)->v[0], (double)i, 0.0);
    kl_io_stream_chunk_free(&s, &chunk);
  }

  kl_io_stream_destroy(&s);
}

// ---------- multi-threaded ----------

typedef struct {
  KL_IOStream* stream;
  int32_t type;
  int64_t sink;
  int64_t expect_count;
  // results
  int64_t received;
  int64_t out_of_order;
  int64_t wrong_type;
  int64_t misaligned;
} KL_TestIOSReader;

static void* kl_test_ios_reader_main(void* arg) {
  KL_TestIOSReader* r = (KL_TestIOSReader*)arg;
  int64_t expected_seq = 0;
  while (r->received < r->expect_count) {
    KL_IOStreamChunk chunk = {0};
    if (!kl_io_stream_read_chunk(r->stream, r->sink, 1.0, &chunk)) {
      sched_yield();
      continue;
    }
    if (chunk.type != r->type) r->wrong_type += 1;
    if (((uintptr_t)chunk.blob.value) % alignof(KL_TestIOSMsgA) != 0) r->misaligned += 1;
    KL_TestIOSMsgA* msg = (KL_TestIOSMsgA*)chunk.blob.value;
    if (msg->seq != expected_seq) r->out_of_order += 1;
    if (msg->tag != r->type) r->wrong_type += 1;
    expected_seq = msg->seq + 1;
    r->received += 1;
    kl_io_stream_chunk_free(r->stream, &chunk);
  }
  return 0;
}

typedef struct {
  KL_IOStream* stream;
  int32_t type;
  int64_t count;
  // A bounded sink can drop; the writer retries a message until every sink
  // took it so the readers see a gap-free sequence.
  int64_t expected_sinks;
  int64_t retries;
} KL_TestIOSWriter;

static void* kl_test_ios_writer_main(void* arg) {
  KL_TestIOSWriter* w = (KL_TestIOSWriter*)arg;
  for (int64_t i = 0; i < w->count; i++) {
    KL_TestIOSMsgA msg = { .seq = i, .tag = w->type };
    while (kl_io_stream_write(w->stream, w->type, KL_BLOB_ALIGNED_RAW_T(KL_TestIOSMsgA, msg)) < w->expected_sinks) {
      // At least one sink was full (or a competing sink hasn't been created yet
      // — it isn't in this test). The full sinks keep their copy, so re-sending
      // would duplicate for them; instead wait until *all* have room. With one
      // sink per type this is exactly "retry until accepted".
      w->retries += 1;
      sched_yield();
    }
  }
  return 0;
}

void test_io_stream_threads_single_writer_many_readers() {
  KL_IOStream s;
  kl_test_ios_setup(&s, 4);

  enum { READERS = 6, COUNT = 20000 };
  int64_t sink_id = kl_io_stream_create_sink(&s, (KL_IOStreamSinkConfig){
    .type = KL_TEST_IOS_TYPE_A, .max_queue_length = COUNT, // unbounded for this test
  });
  KL_TEST_EXPECT_EXPR(sink_id >= 0);

  // One sink per reader so each reader observes the full sequence.
  int64_t sinks[READERS];
  sinks[0] = sink_id;
  for (int i = 1; i < READERS; i++) {
    sinks[i] = kl_io_stream_create_sink(&s, (KL_IOStreamSinkConfig){
      .type = KL_TEST_IOS_TYPE_A, .max_queue_length = COUNT,
    });
    KL_TEST_EXPECT_EXPR(sinks[i] >= 0);
  }

  KL_TestIOSReader readers[READERS] = {0};
  pthread_t reader_threads[READERS];
  for (int i = 0; i < READERS; i++) {
    readers[i] = (KL_TestIOSReader){ .stream = &s, .type = KL_TEST_IOS_TYPE_A, .sink = sinks[i], .expect_count = COUNT };
    KL_TEST_EXPECT_EQ_INT(pthread_create(&reader_threads[i], 0, kl_test_ios_reader_main, &readers[i]), 0);
  }

  KL_TestIOSWriter writer = { .stream = &s, .type = KL_TEST_IOS_TYPE_A, .count = COUNT, .expected_sinks = READERS };
  pthread_t writer_thread;
  KL_TEST_EXPECT_EQ_INT(pthread_create(&writer_thread, 0, kl_test_ios_writer_main, &writer), 0);

  pthread_join(writer_thread, 0);
  for (int i = 0; i < READERS; i++) pthread_join(reader_threads[i], 0);

  KL_TEST_EXPECT_EQ_INT(writer.retries, 0); // queues were big enough, nothing dropped
  for (int i = 0; i < READERS; i++) {
    KL_TEST_EXPECT_EQ_INT(readers[i].received, COUNT);
    KL_TEST_EXPECT_EQ_INT(readers[i].out_of_order, 0);
    KL_TEST_EXPECT_EQ_INT(readers[i].wrong_type, 0);
    KL_TEST_EXPECT_EQ_INT(readers[i].misaligned, 0);
    KL_TEST_EXPECT_EQ_INT(kl_io_stream_sink_length(&s, sinks[i]), 0);
  }
  // every chunk that was handed out was freed again
  KL_TEST_EXPECT_EQ_INT(kl_test_ios_live_blobs(&s), 0);

  kl_io_stream_destroy(&s);
}

void test_io_stream_threads_bounded_sink_backpressure() {
  KL_IOStream s;
  kl_test_ios_setup(&s, 4);

  enum { COUNT = 20000 };
  // A tiny queue: the writer will run into a full sink constantly and has to
  // retry; the reader must still see every message exactly once, in order.
  int64_t sink = kl_io_stream_create_sink(&s, (KL_IOStreamSinkConfig){
    .type = KL_TEST_IOS_TYPE_A, .max_queue_length = 4,
  });

  KL_TestIOSReader reader = { .stream = &s, .type = KL_TEST_IOS_TYPE_A, .sink = sink, .expect_count = COUNT };
  KL_TestIOSWriter writer = { .stream = &s, .type = KL_TEST_IOS_TYPE_A, .count = COUNT, .expected_sinks = 1 };

  pthread_t rt, wt;
  KL_TEST_EXPECT_EQ_INT(pthread_create(&rt, 0, kl_test_ios_reader_main, &reader), 0);
  KL_TEST_EXPECT_EQ_INT(pthread_create(&wt, 0, kl_test_ios_writer_main, &writer), 0);
  pthread_join(wt, 0);
  pthread_join(rt, 0);

  KL_TEST_EXPECT_EQ_INT(reader.received, COUNT);
  KL_TEST_EXPECT_EQ_INT(reader.out_of_order, 0);
  KL_TEST_EXPECT_EQ_INT(reader.wrong_type, 0);
  KL_TEST_EXPECT_EQ_INT(kl_io_stream_sink_length(&s, sink), 0);

  kl_io_stream_destroy(&s);
}

void test_io_stream_threads_multiple_writers_multiple_types() {
  KL_IOStream s;
  kl_test_ios_setup(&s, 4);
  // type B reuses the MsgA layout for this test
  kl_io_stream_create_funnel(&s, (KL_IOStreamSinkFunnelConfig){
    .type = 3, .chunk = { .size = sizeof(KL_TestIOSMsgA), .alignment = alignof(KL_TestIOSMsgA) },
  });
  kl_io_stream_create_funnel(&s, (KL_IOStreamSinkFunnelConfig){
    .type = 4, .chunk = { .size = sizeof(KL_TestIOSMsgA), .alignment = alignof(KL_TestIOSMsgA) },
  });

  enum { TYPES = 3, COUNT = 10000 };
  const int32_t types[TYPES] = { KL_TEST_IOS_TYPE_A, 3, 4 };

  KL_TestIOSReader readers[TYPES] = {0};
  KL_TestIOSWriter writers[TYPES] = {0};
  pthread_t rts[TYPES], wts[TYPES];

  for (int i = 0; i < TYPES; i++) {
    int64_t sink = kl_io_stream_create_sink(&s, (KL_IOStreamSinkConfig){ .type = types[i], .max_queue_length = 16 });
    KL_TEST_EXPECT_EXPR(sink >= 0);
    readers[i] = (KL_TestIOSReader){ .stream = &s, .type = types[i], .sink = sink, .expect_count = COUNT };
    writers[i] = (KL_TestIOSWriter){ .stream = &s, .type = types[i], .count = COUNT, .expected_sinks = 1 };
  }
  for (int i = 0; i < TYPES; i++) {
    KL_TEST_EXPECT_EQ_INT(pthread_create(&rts[i], 0, kl_test_ios_reader_main, &readers[i]), 0);
    KL_TEST_EXPECT_EQ_INT(pthread_create(&wts[i], 0, kl_test_ios_writer_main, &writers[i]), 0);
  }
  for (int i = 0; i < TYPES; i++) {
    pthread_join(wts[i], 0);
    pthread_join(rts[i], 0);
  }
  for (int i = 0; i < TYPES; i++) {
    KL_TEST_EXPECT_EQ_INT(readers[i].received, COUNT);
    KL_TEST_EXPECT_EQ_INT(readers[i].out_of_order, 0);
    KL_TEST_EXPECT_EQ_INT(readers[i].wrong_type, 0);
  }

  kl_io_stream_destroy(&s);
}

typedef struct {
  KL_IOStream* stream;
  volatile bool* stop;
  int64_t writes;
} KL_TestIOSFreeWriter;

static void* kl_test_ios_free_writer_main(void* arg) {
  KL_TestIOSFreeWriter* w = (KL_TestIOSFreeWriter*)arg;
  int64_t seq = 0;
  while (!*w->stop) {
    KL_TestIOSMsgA msg = { .seq = seq++, .tag = KL_TEST_IOS_TYPE_A };
    int64_t r = kl_io_stream_write(w->stream, KL_TEST_IOS_TYPE_A, KL_BLOB_ALIGNED_RAW_T(KL_TestIOSMsgA, msg));
    if (r < 0) { w->writes = -1; return 0; }
    w->writes += 1;
  }
  return 0;
}

void test_io_stream_threads_create_close_sinks_while_writing() {
  KL_IOStream s;
  kl_test_ios_setup(&s, 2); // small pool so it grows under load

  volatile bool stop = false;
  KL_TestIOSFreeWriter writer = { .stream = &s, .stop = &stop };
  pthread_t wt;
  KL_TEST_EXPECT_EQ_INT(pthread_create(&wt, 0, kl_test_ios_free_writer_main, &writer), 0);

  // Churn sinks on this thread while the writer broadcasts non-stop: open a
  // handful, read whatever landed in them, close them, repeat.
  enum { ROUNDS = 300, PER_ROUND = 5 };
  int64_t total_read = 0;
  for (int round = 0; round < ROUNDS; round++) {
    int64_t sinks[PER_ROUND];
    for (int i = 0; i < PER_ROUND; i++) {
      sinks[i] = kl_io_stream_create_sink(&s, (KL_IOStreamSinkConfig){
        .type = KL_TEST_IOS_TYPE_A, .max_queue_length = 8,
      });
      KL_TEST_EXPECT_EXPR(sinks[i] >= 0);
    }
    for (int i = 0; i < PER_ROUND; i++) {
      int64_t last_seq = -1;
      KL_IOStreamChunk chunk = {0};
      while (kl_io_stream_read_chunk(&s, sinks[i], 0, &chunk)) {
        KL_TestIOSMsgA* msg = (KL_TestIOSMsgA*)chunk.blob.value;
        KL_TEST_EXPECT_EQ_INT(chunk.type, KL_TEST_IOS_TYPE_A);
        KL_TEST_EXPECT_EXPR(msg->seq > last_seq); // gaps allowed (drops), reordering not
        last_seq = msg->seq;
        total_read += 1;
        kl_io_stream_chunk_free(&s, &chunk);
      }
      // close with whatever is still queued (exercises free-on-close under a live writer)
      kl_io_stream_close_sink(&s, sinks[i]);
      KL_TEST_EXPECT_EXPR(kl_io_stream_get_sink(&s, sinks[i]) == 0);
    }
  }

  stop = true;
  pthread_join(wt, 0);

  KL_TEST_EXPECT_EXPR(writer.writes > 0);
  KL_TEST_EXPECT_EXPR(total_read > 0);
  KL_IOStreamSinkFunnel* funnel = kl_io_stream_get_funnel(&s, KL_TEST_IOS_TYPE_A);
  KL_TEST_EXPECT_EQ_INT(funnel->sinks.taken_indices.length, 0);
  // no sinks left → nothing may still be allocated
  KL_TEST_EXPECT_EQ_INT(kl_test_ios_live_blobs(&s), 0);

  kl_io_stream_destroy(&s);
}

void test_io_stream_read_chunks() {
  KL_IOStream s;
  kl_test_ios_setup(&s, 4);
  int64_t sink = kl_io_stream_create_sink(&s, (KL_IOStreamSinkConfig){
    .type = KL_TEST_IOS_TYPE_A, .max_queue_length = 64,
  });

  KL_io_stream_chunk_List out = {0};
  kl_io_stream_chunk_list_init(&out, (KL_ListConfig){ .capacity = 4 });

  // empty sink: nothing read, list untouched
  KL_TEST_EXPECT_EXPR(!kl_io_stream_read_chunks(&s, sink, 0, 8, &out));
  KL_TEST_EXPECT_EQ_INT(out.length, 0);

  for (int64_t i = 0; i < 10; i++) KL_TEST_EXPECT_EQ_INT(kl_test_ios_write_a(&s, i, 0), 1);

  // limit smaller than what's queued: takes exactly `limit`, oldest first
  KL_TEST_EXPECT_EXPR(kl_io_stream_read_chunks(&s, sink, 0, 4, &out));
  KL_TEST_EXPECT_EQ_INT(out.length, 4);
  KL_TEST_EXPECT_EQ_INT(kl_io_stream_sink_length(&s, sink), 6);
  for (int64_t i = 0; i < 4; i++) {
    KL_TEST_EXPECT_EQ_INT(out.items[i].type, KL_TEST_IOS_TYPE_A);
    KL_TEST_EXPECT_EQ_INT(out.items[i].blob.size, (int64_t)sizeof(KL_TestIOSMsgA));
    KL_TEST_EXPECT_EQ_INT(((KL_TestIOSMsgA*)out.items[i].blob.value)->seq, i);
  }

  // appends to whatever is already in the list; limit larger than remaining
  // returns only what's there and leaves the sink empty
  KL_TEST_EXPECT_EXPR(kl_io_stream_read_chunks(&s, sink, 0, 100, &out));
  KL_TEST_EXPECT_EQ_INT(out.length, 10);
  KL_TEST_EXPECT_EQ_INT(kl_io_stream_sink_length(&s, sink), 0);
  for (int64_t i = 0; i < 10; i++) {
    KL_TEST_EXPECT_EQ_INT(((KL_TestIOSMsgA*)out.items[i].blob.value)->seq, i);
  }

  // every chunk is owned by the caller
  KL_TEST_EXPECT_EQ_INT(kl_test_ios_live_blobs(&s), 10);
  for (int64_t i = 0; i < out.length; i++) kl_io_stream_chunk_free(&s, &out.items[i]);
  kl_io_stream_chunk_list_reset(&out);
  KL_TEST_EXPECT_EQ_INT(kl_test_ios_live_blobs(&s), 0);

  // limit <= 0 reads nothing even when data is queued
  KL_TEST_EXPECT_EQ_INT(kl_test_ios_write_a(&s, 42, 0), 1);
  KL_TEST_EXPECT_EXPR(!kl_io_stream_read_chunks(&s, sink, 0, 0, &out));
  KL_TEST_EXPECT_EXPR(!kl_io_stream_read_chunks(&s, sink, 0, -1, &out));
  KL_TEST_EXPECT_EQ_INT(out.length, 0);
  KL_TEST_EXPECT_EQ_INT(kl_io_stream_sink_length(&s, sink), 1);

  // unknown sink: false, list untouched
  KL_TEST_EXPECT_EXPR(!kl_io_stream_read_chunks(&s, 4242, 0, 8, &out));
  KL_TEST_EXPECT_EQ_INT(out.length, 0);

  // with a lock timeout it behaves the same as a plain poll
  KL_TEST_EXPECT_EXPR(kl_io_stream_read_chunks(&s, sink, 1.0, 8, &out));
  KL_TEST_EXPECT_EQ_INT(out.length, 1);
  KL_TEST_EXPECT_EQ_INT(((KL_TestIOSMsgA*)out.items[0].blob.value)->seq, 42);
  kl_io_stream_chunk_free(&s, &out.items[0]);

  kl_io_stream_chunk_list_destroy(&out);
  kl_io_stream_destroy(&s);
}

// ---------- write_timeout ----------

typedef struct {
  KL_IOStream* stream;
  double timeout_ms;
  int64_t result;
} KL_TestIOSTimeoutWriter;

static void* kl_test_ios_timeout_writer_main(void* arg) {
  KL_TestIOSTimeoutWriter* w = (KL_TestIOSTimeoutWriter*)arg;
  KL_TestIOSMsgA msg = { .seq = 77, .tag = 0 };
  w->result = kl_io_stream_write_timeout(w->stream, KL_TEST_IOS_TYPE_A,
    KL_BLOB_ALIGNED_RAW_T(KL_TestIOSMsgA, msg), w->timeout_ms);
  return 0;
}

// Runs write_timeout on a separate thread (the kl mutex is recursive, so a
// lock held by *this* thread would not block a write from this thread).
static int64_t kl_test_ios_write_timeout_from_thread(KL_IOStream* s, double timeout_ms) {
  KL_TestIOSTimeoutWriter w = { .stream = s, .timeout_ms = timeout_ms, .result = -99 };
  pthread_t t;
  KL_TEST_EXPECT_EQ_INT(pthread_create(&t, 0, kl_test_ios_timeout_writer_main, &w), 0);
  pthread_join(t, 0);
  return w.result;
}

void test_io_stream_write_timeout_uncontended_behaves_like_write() {
  KL_IOStream s;
  kl_test_ios_setup(&s, 4);

  KL_TestIOSMsgA msg = { .seq = 5, .tag = 1 };
  KL_BlobAligned data = KL_BLOB_ALIGNED_RAW_T(KL_TestIOSMsgA, msg);

  KL_TEST_EXPECT_EQ_INT(kl_io_stream_write_timeout(&s, KL_TEST_IOS_TYPE_UNKNOWN, data, 1.0), -1);
  KL_TEST_EXPECT_EQ_INT(kl_io_stream_write_timeout(&s, KL_TEST_IOS_TYPE_A, data, 1.0), 0); // no sinks

  int64_t a = kl_io_stream_create_sink(&s, (KL_IOStreamSinkConfig){ .type = KL_TEST_IOS_TYPE_A, .max_queue_length = 2 });
  int64_t b = kl_io_stream_create_sink(&s, (KL_IOStreamSinkConfig){ .type = KL_TEST_IOS_TYPE_A, .max_queue_length = 8 });

  KL_TEST_EXPECT_EQ_INT(kl_io_stream_write_timeout(&s, KL_TEST_IOS_TYPE_A, data, 1.0), 2);
  KL_TEST_EXPECT_EQ_INT(kl_io_stream_write_timeout(&s, KL_TEST_IOS_TYPE_A, data, 0.0), 2);  // 0 → trylock
  KL_TEST_EXPECT_EQ_INT(kl_io_stream_write_timeout(&s, KL_TEST_IOS_TYPE_A, data, -1.0), 1); // a is full now
  KL_TEST_EXPECT_EQ_INT(kl_io_stream_sink_length(&s, a), 2);
  KL_TEST_EXPECT_EQ_INT(kl_io_stream_sink_length(&s, b), 3);
  KL_TEST_EXPECT_EQ_INT(kl_test_ios_live_blobs(&s), 5);

  KL_IOStreamChunk chunk = {0};
  KL_TEST_EXPECT_EXPR(kl_io_stream_read_chunk(&s, b, 0, &chunk));
  KL_TEST_EXPECT_EQ_INT(chunk.type, KL_TEST_IOS_TYPE_A);
  KL_TEST_EXPECT_EQ_INT(((KL_TestIOSMsgA*)chunk.blob.value)->seq, 5);
  KL_TEST_EXPECT_EQ_INT(((KL_TestIOSMsgA*)chunk.blob.value)->tag, 1);
  kl_io_stream_chunk_free(&s, &chunk);

  kl_io_stream_destroy(&s);
}

void test_io_stream_write_timeout_gives_up_when_funnel_is_locked() {
  KL_IOStream s;
  kl_test_ios_setup(&s, 4);
  int64_t sink = kl_io_stream_create_sink(&s, (KL_IOStreamSinkConfig){ .type = KL_TEST_IOS_TYPE_A, .max_queue_length = 8 });
  KL_IOStreamSinkFunnel* funnel = kl_io_stream_get_funnel(&s, KL_TEST_IOS_TYPE_A);

  // Simulate a stalled create/close/write holding the funnel's sink-set lock.
  kl_mutex_lock(&funnel->lock);

  long long t0 = kl_test_get_time_us();
  KL_TEST_EXPECT_EQ_INT(kl_test_ios_write_timeout_from_thread(&s, 5.0), -2);
  long long elapsed_us = kl_test_get_time_us() - t0;
  KL_TEST_ASSERT(elapsed_us >= 5000, "must have waited for the timeout before giving up");
  KL_TEST_ASSERT(elapsed_us < 1000000, "must not block indefinitely");

  // trylock variant (timeout <= 0) also gives up, immediately
  KL_TEST_EXPECT_EQ_INT(kl_test_ios_write_timeout_from_thread(&s, 0.0), -2);

  // nothing was written, nothing leaked
  KL_TEST_EXPECT_EQ_INT(kl_io_stream_sink_length(&s, sink), 0);
  KL_TEST_EXPECT_EQ_INT(kl_test_ios_live_blobs(&s), 0);

  kl_mutex_unlock(&funnel->lock);

  // once released, the same write goes through
  KL_TEST_EXPECT_EQ_INT(kl_test_ios_write_timeout_from_thread(&s, 5.0), 1);
  KL_TEST_EXPECT_EQ_INT(kl_io_stream_sink_length(&s, sink), 1);

  kl_io_stream_destroy(&s);
}

void test_io_stream_write_timeout_drops_only_the_locked_sink() {
  KL_IOStream s;
  kl_test_ios_setup(&s, 4);
  int64_t busy = kl_io_stream_create_sink(&s, (KL_IOStreamSinkConfig){ .type = KL_TEST_IOS_TYPE_A, .max_queue_length = 8 });
  int64_t free_ = kl_io_stream_create_sink(&s, (KL_IOStreamSinkConfig){ .type = KL_TEST_IOS_TYPE_A, .max_queue_length = 8 });
  KL_IOStreamSink* busy_sink = kl_io_stream_get_sink(&s, busy);

  // Simulate a reader that is stuck inside a pop on one sink.
  kl_mutex_lock(&busy_sink->queue.lock);

  KL_TEST_EXPECT_EQ_INT(kl_test_ios_write_timeout_from_thread(&s, 5.0), 1);
  KL_TEST_EXPECT_EQ_INT(kl_test_ios_write_timeout_from_thread(&s, 0.0), 1);

  kl_mutex_unlock(&busy_sink->queue.lock);

  KL_TEST_EXPECT_EQ_INT(kl_io_stream_sink_length(&s, busy), 0);
  KL_TEST_EXPECT_EQ_INT(kl_io_stream_sink_length(&s, free_), 2);
  // the two copies dropped for the busy sink were freed
  KL_TEST_EXPECT_EQ_INT(kl_test_ios_live_blobs(&s), 2);

  // the funnel lock was released again: a plain blocking write works
  KL_TEST_EXPECT_EQ_INT(kl_test_ios_write_a(&s, 1, 0), 2);
  KL_TEST_EXPECT_EQ_INT(kl_io_stream_sink_length(&s, busy), 1);

  kl_io_stream_destroy(&s);
}

void test_io_stream_sink_ids_unique_across_funnels() {
  KL_IOStream s;
  kl_test_ios_setup(&s, 4);

  // Both are slot 0 of their own funnel's pool; the public ids must still differ
  // and each id must route to its own funnel without the caller naming the type.
  int64_t a = kl_io_stream_create_sink(&s, (KL_IOStreamSinkConfig){ .type = KL_TEST_IOS_TYPE_A, .max_queue_length = 4 });
  int64_t b = kl_io_stream_create_sink(&s, (KL_IOStreamSinkConfig){ .type = KL_TEST_IOS_TYPE_B, .max_queue_length = 4 });
  KL_TEST_EXPECT_EXPR(a >= 0);
  KL_TEST_EXPECT_EXPR(b >= 0);
  KL_TEST_EXPECT_EXPR(a != b);

  KL_IOStreamSink* sa = kl_io_stream_get_sink(&s, a);
  KL_IOStreamSink* sb = kl_io_stream_get_sink(&s, b);
  KL_TEST_EXPECT_EXPR(sa != 0 && sb != 0 && sa != sb);
  KL_TEST_EXPECT_EQ_INT(sa->config.type, KL_TEST_IOS_TYPE_A);
  KL_TEST_EXPECT_EQ_INT(sb->config.type, KL_TEST_IOS_TYPE_B);
  KL_TEST_EXPECT_EQ_INT(sa->id, a);
  KL_TEST_EXPECT_EQ_INT(sb->id, b);
  KL_TEST_EXPECT_EQ_INT(sa->pool_handle, sb->pool_handle);

  KL_TEST_EXPECT_EQ_INT(kl_test_ios_write_a(&s, 1, 0), 1);
  KL_TestIOSMsgB mb = { .v = { 1, 2, 3, 4 } };
  KL_TEST_EXPECT_EQ_INT(kl_io_stream_write(&s, KL_TEST_IOS_TYPE_B, KL_BLOB_ALIGNED_RAW_T(KL_TestIOSMsgB, mb)), 1);

  KL_IOStreamChunk chunk = {0};
  KL_TEST_EXPECT_EXPR(kl_io_stream_read_chunk(&s, b, 0, &chunk));
  KL_TEST_EXPECT_EQ_INT(chunk.type, KL_TEST_IOS_TYPE_B);
  KL_TEST_EXPECT_EQ_INT(chunk.blob.size, (int64_t)sizeof(KL_TestIOSMsgB));
  kl_io_stream_chunk_free(&s, &chunk);
  KL_TEST_EXPECT_EXPR(kl_io_stream_read_chunk(&s, a, 0, &chunk));
  KL_TEST_EXPECT_EQ_INT(chunk.type, KL_TEST_IOS_TYPE_A);
  kl_io_stream_chunk_free(&s, &chunk);

  // Closing a and re-creating in the same funnel recycles the pool slot but
  // never the public id: the old id stays dead.
  kl_io_stream_close_sink(&s, a);
  int64_t a2 = kl_io_stream_create_sink(&s, (KL_IOStreamSinkConfig){ .type = KL_TEST_IOS_TYPE_A, .max_queue_length = 4 });
  KL_TEST_EXPECT_EXPR(a2 >= 0 && a2 != a && a2 != b);
  KL_TEST_EXPECT_EXPR(kl_io_stream_get_sink(&s, a) == 0);
  KL_TEST_EXPECT_EQ_INT(kl_io_stream_sink_length(&s, a), -1);
  KL_TEST_EXPECT_EXPR(!kl_io_stream_read_chunk(&s, a, 0, &chunk));
  KL_TEST_EXPECT_EQ_INT(kl_io_stream_sink_length(&s, a2), 0);
  KL_TEST_EXPECT_EQ_INT(kl_test_ios_write_a(&s, 2, 0), 1);
  KL_TEST_EXPECT_EQ_INT(kl_io_stream_sink_length(&s, a2), 1);
  KL_TEST_EXPECT_EQ_INT(kl_io_stream_sink_length(&s, b), 0);

  kl_io_stream_destroy(&s);
}

// ---------- stress: thousands of sinks churned per frame ----------
//
// Models a real-time loop: writers broadcast in short bursts (like a physics
// tick that emits a handful of events then moves on) while several "systems"
// (worker threads) each open a batch of sinks at the start of a frame, read
// whatever landed, and close them again before the next frame.
//
// NOTE: the writers deliberately pause between bursts. A writer that spins on
// `kl_io_stream_write` without ever leaving the funnel lock idle starves
// create/close_sink (the lock is not fair, and a broadcast to N sinks holds it
// for O(N)); that is a known limitation, not something this test measures. Verifies
// correctness from the worker side (type/size/alignment/order, stale ids never
// resolve), id uniqueness across the whole run, that every resource is back
// where it started once the sinks are gone, and that the stream's footprint is
// bounded by the peak number of live sinks rather than by the total churned.

enum {
  KL_TEST_IOS_STRESS_WORKERS_PER_TYPE = 4,
  KL_TEST_IOS_STRESS_SINKS_PER_FRAME = 250,   // per worker → 2000 live sinks per frame
  KL_TEST_IOS_STRESS_FRAMES = 6,
  KL_TEST_IOS_STRESS_QUEUE_LENGTH = 4,
  KL_TEST_IOS_STRESS_WRITER_BURST = 8,        // writes per burst
  KL_TEST_IOS_STRESS_WRITER_PAUSE_US = 50,    // idle between bursts (~20k bursts/s, far above any tick rate)
};

typedef struct {
  KL_IOStream* stream;
  int32_t type;
  int64_t chunk_size;
  pthread_barrier_t* frame_barrier;
  // results
  int64_t created;
  int64_t create_failures;
  int64_t closed;
  int64_t chunks_read;
  int64_t wrong_type;
  int64_t wrong_size;
  int64_t misaligned;
  int64_t reordered;
  int64_t stale_resolved;   // a closed id that still resolved / still read
  int64_t never_delivered;  // frames where the writer never reached this worker's first sink
  double create_ms;
  double close_ms;
  double read_ms;
  KL_int64_List ids;        // every id this worker was ever handed
} KL_TestIOSStressWorker;

static void* kl_test_ios_stress_worker_main(void* arg) {
  KL_TestIOSStressWorker* w = (KL_TestIOSStressWorker*)arg;
  int64_t sinks[KL_TEST_IOS_STRESS_SINKS_PER_FRAME];

  for (int frame = 0; frame < KL_TEST_IOS_STRESS_FRAMES; frame++) {
    pthread_barrier_wait(w->frame_barrier); // frame start

    double t0 = kl_time_now_ms__monotonic();
    for (int i = 0; i < KL_TEST_IOS_STRESS_SINKS_PER_FRAME; i++) {
      sinks[i] = kl_io_stream_create_sink(w->stream, (KL_IOStreamSinkConfig){
        .type = w->type, .max_queue_length = KL_TEST_IOS_STRESS_QUEUE_LENGTH,
      });
      if (sinks[i] < 0) { w->create_failures += 1; continue; }
      w->created += 1;
      kl_int64_list_push(&w->ids, sinks[i]);
    }
    double t1 = kl_time_now_ms__monotonic();

    // Wait (bounded) until the writer has reached at least the first sink of
    // this batch, so "the writers deliver to every worker every frame" holds
    // regardless of scheduling; then read every sink once.
    {
      double deadline = kl_time_now_ms__monotonic() + 500.0;
      while (sinks[0] >= 0 && kl_io_stream_sink_length(w->stream, sinks[0]) <= 0) {
        if (kl_time_now_ms__monotonic() > deadline) { w->never_delivered += 1; break; }
        sched_yield();
      }
    }
    for (int i = 0; i < KL_TEST_IOS_STRESS_SINKS_PER_FRAME; i++) {
      if (sinks[i] < 0) continue;
      int64_t last_seq = -1;
      KL_IOStreamChunk chunk = {0};
      while (kl_io_stream_read_chunk(w->stream, sinks[i], 0, &chunk)) {
        if (chunk.type != w->type) w->wrong_type += 1;
        if (chunk.blob.size != w->chunk_size) w->wrong_size += 1;
        if (((uintptr_t)chunk.blob.value) % 32 != 0 && w->type == KL_TEST_IOS_TYPE_B) w->misaligned += 1;
        if (w->type == KL_TEST_IOS_TYPE_A) {
          KL_TestIOSMsgA* msg = (KL_TestIOSMsgA*)chunk.blob.value;
          if (msg->tag != w->type) w->wrong_type += 1;
          if (msg->seq <= last_seq) w->reordered += 1; // gaps allowed (drops), reordering not
          last_seq = msg->seq;
        }
        w->chunks_read += 1;
        kl_io_stream_chunk_free(w->stream, &chunk);
      }
    }
    double t2 = kl_time_now_ms__monotonic();

    // Close in reverse so the pool's free list and the ref map see a different
    // order than they were filled in. Sinks still hold queued chunks here, so
    // this also exercises free-on-close under live writers.
    for (int i = KL_TEST_IOS_STRESS_SINKS_PER_FRAME - 1; i >= 0; i--) {
      if (sinks[i] < 0) continue;
      kl_io_stream_close_sink(w->stream, sinks[i]);
      w->closed += 1;
    }
    double t3 = kl_time_now_ms__monotonic();

    // Every id from this frame must now be dead, even though the pool slots
    // are already being recycled by other workers.
    for (int i = 0; i < KL_TEST_IOS_STRESS_SINKS_PER_FRAME; i++) {
      if (sinks[i] < 0) continue;
      KL_IOStreamChunk chunk = {0};
      if (kl_io_stream_get_sink(w->stream, sinks[i]) != 0) w->stale_resolved += 1;
      if (kl_io_stream_sink_length(w->stream, sinks[i]) != -1) w->stale_resolved += 1;
      if (kl_io_stream_read_chunk(w->stream, sinks[i], 0, &chunk)) w->stale_resolved += 1;
    }

    w->create_ms += t1 - t0;
    w->read_ms += t2 - t1;
    w->close_ms += t3 - t2;

    pthread_barrier_wait(w->frame_barrier); // frame end
  }
  return 0;
}

typedef struct {
  KL_IOStream* stream;
  int32_t type;
  volatile bool* stop;
  int64_t writes;
  int64_t accepted;
  int64_t errors;
} KL_TestIOSStressWriter;

static void* kl_test_ios_stress_writer_main(void* arg) {
  KL_TestIOSStressWriter* w = (KL_TestIOSStressWriter*)arg;
  int64_t seq = 0;
  while (!*w->stop) {
    if (seq % KL_TEST_IOS_STRESS_WRITER_BURST == 0) {
      struct timespec pause = { .tv_sec = 0, .tv_nsec = KL_TEST_IOS_STRESS_WRITER_PAUSE_US * 1000L };
      nanosleep(&pause, 0);
    }
    int64_t r;
    if (w->type == KL_TEST_IOS_TYPE_A) {
      KL_TestIOSMsgA msg = { .seq = seq++, .tag = KL_TEST_IOS_TYPE_A };
      r = kl_io_stream_write(w->stream, KL_TEST_IOS_TYPE_A, KL_BLOB_ALIGNED_RAW_T(KL_TestIOSMsgA, msg));
    } else {
      KL_TestIOSMsgB msg = { .v = { (double)seq, 1, 2, 3 } };
      seq++;
      r = kl_io_stream_write(w->stream, KL_TEST_IOS_TYPE_B, KL_BLOB_ALIGNED_RAW_T(KL_TestIOSMsgB, msg));
    }
    if (r < 0) { w->errors += 1; continue; }
    w->writes += 1;
    w->accepted += r;
  }
  return 0;
}

static int kl_test_ios_cmp_int64(const void* a, const void* b) {
  int64_t x = *(const int64_t*)a, y = *(const int64_t*)b;
  return (x > y) - (x < y);
}

static int64_t kl_test_ios_sink_ref_count(KL_IOStream* s) {
  int64_t n = 0;
  for (int64_t i = 0; i < s->sink_refs.slots.length; i++) {
    if (s->sink_refs.slots.items[i].is_set) n += 1;
  }
  return n;
}

void test_io_stream_stress_sink_churn_per_frame() {
  enum {
    WORKERS = KL_TEST_IOS_STRESS_WORKERS_PER_TYPE * 2,
    PEAK_LIVE_SINKS_PER_TYPE = KL_TEST_IOS_STRESS_WORKERS_PER_TYPE * KL_TEST_IOS_STRESS_SINKS_PER_FRAME,
    PEAK_LIVE_SINKS = PEAK_LIVE_SINKS_PER_TYPE * 2,
    TOTAL_SINKS = PEAK_LIVE_SINKS * KL_TEST_IOS_STRESS_FRAMES,
  };

  KL_IOStream s;
  kl_test_ios_setup(&s, 16); // tiny initial pool + arena so everything has to grow under load

  volatile bool stop = false;
  KL_TestIOSStressWriter writers[2] = {
    { .stream = &s, .type = KL_TEST_IOS_TYPE_A, .stop = &stop },
    { .stream = &s, .type = KL_TEST_IOS_TYPE_B, .stop = &stop },
  };
  pthread_t wts[2];
  for (int i = 0; i < 2; i++) {
    KL_TEST_EXPECT_EQ_INT(pthread_create(&wts[i], 0, kl_test_ios_stress_writer_main, &writers[i]), 0);
  }

  pthread_barrier_t barrier;
  pthread_barrier_init(&barrier, 0, WORKERS + 1); // +1: this thread samples between frames

  KL_TestIOSStressWorker workers[WORKERS] = {0};
  pthread_t ts[WORKERS];
  for (int i = 0; i < WORKERS; i++) {
    bool is_a = i < KL_TEST_IOS_STRESS_WORKERS_PER_TYPE;
    workers[i] = (KL_TestIOSStressWorker){
      .stream = &s,
      .type = is_a ? KL_TEST_IOS_TYPE_A : KL_TEST_IOS_TYPE_B,
      .chunk_size = is_a ? (int64_t)sizeof(KL_TestIOSMsgA) : (int64_t)sizeof(KL_TestIOSMsgB),
      .frame_barrier = &barrier,
    };
    kl_int64_list_init(&workers[i].ids, (KL_ListConfig){ .capacity = KL_TEST_IOS_STRESS_SINKS_PER_FRAME * KL_TEST_IOS_STRESS_FRAMES });
    KL_TEST_EXPECT_EQ_INT(pthread_create(&ts[i], 0, kl_test_ios_stress_worker_main, &workers[i]), 0);
  }

  // Drive the frames. Between frames every sink is closed, so the stream must
  // be back at "empty" each time. (Capacity is allowed to keep growing towards
  // the per-frame peak for a few frames — how many sinks are live at once
  // depends on thread timing — the bounds at the end cap it.)
  double frame_ms_max = 0;
  double t_start = kl_time_now_ms__monotonic();
  for (int frame = 0; frame < KL_TEST_IOS_STRESS_FRAMES; frame++) {
    double f0 = kl_time_now_ms__monotonic();
    pthread_barrier_wait(&barrier); // frame start
    pthread_barrier_wait(&barrier); // frame end
    double f1 = kl_time_now_ms__monotonic();
    if (f1 - f0 > frame_ms_max) frame_ms_max = f1 - f0;

    KL_IOStreamSinkFunnel* fa = kl_io_stream_get_funnel(&s, KL_TEST_IOS_TYPE_A);
    KL_IOStreamSinkFunnel* fb = kl_io_stream_get_funnel(&s, KL_TEST_IOS_TYPE_B);
    KL_TEST_EXPECT_EQ_INT(fa->sinks.taken_indices.length, 0);
    KL_TEST_EXPECT_EQ_INT(fb->sinks.taken_indices.length, 0);
    KL_TEST_EXPECT_EQ_INT(kl_test_ios_sink_ref_count(&s), 0);
    KL_TEST_EXPECT_EQ_INT(kl_test_ios_live_blobs(&s), 0);
  }
  double total_ms = kl_time_now_ms__monotonic() - t_start;

  for (int i = 0; i < WORKERS; i++) pthread_join(ts[i], 0);
  stop = true;
  for (int i = 0; i < 2; i++) pthread_join(wts[i], 0);
  pthread_barrier_destroy(&barrier);

  // ---- behaviour ----
  int64_t created = 0, closed = 0, chunks_read = 0;
  double create_ms = 0, close_ms = 0, read_ms = 0;
  KL_int64_List all_ids = {0};
  kl_int64_list_init(&all_ids, (KL_ListConfig){ .capacity = TOTAL_SINKS });
  for (int i = 0; i < WORKERS; i++) {
    KL_TestIOSStressWorker* w = &workers[i];
    KL_TEST_EXPECT_EQ_INT(w->create_failures, 0);
    KL_TEST_EXPECT_EQ_INT(w->created, KL_TEST_IOS_STRESS_SINKS_PER_FRAME * KL_TEST_IOS_STRESS_FRAMES);
    KL_TEST_EXPECT_EQ_INT(w->closed, w->created);
    KL_TEST_EXPECT_EQ_INT(w->wrong_type, 0);
    KL_TEST_EXPECT_EQ_INT(w->wrong_size, 0);
    KL_TEST_EXPECT_EQ_INT(w->misaligned, 0);
    KL_TEST_EXPECT_EQ_INT(w->reordered, 0);
    KL_TEST_EXPECT_EQ_INT(w->stale_resolved, 0);
    KL_TEST_EXPECT_EQ_INT(w->never_delivered, 0);
    KL_TEST_EXPECT_EXPR(w->chunks_read > 0); // the writers did reach this worker's sinks
    created += w->created; closed += w->closed; chunks_read += w->chunks_read;
    create_ms += w->create_ms; close_ms += w->close_ms; read_ms += w->read_ms;
    for (int64_t j = 0; j < w->ids.length; j++) kl_int64_list_push(&all_ids, w->ids.items[j]);
    kl_int64_list_destroy(&w->ids);
  }
  KL_TEST_EXPECT_EQ_INT(created, TOTAL_SINKS);
  KL_TEST_EXPECT_EQ_INT(closed, TOTAL_SINKS);
  for (int i = 0; i < 2; i++) {
    KL_TEST_EXPECT_EQ_INT(writers[i].errors, 0);
    KL_TEST_EXPECT_EXPR(writers[i].writes > 0);
    KL_TEST_EXPECT_EXPR(writers[i].accepted > 0);
  }

  // Every id handed out during the run is unique, even though pool slots were
  // recycled thousands of times.
  KL_TEST_EXPECT_EQ_INT(all_ids.length, TOTAL_SINKS);
  qsort(all_ids.items, (size_t)all_ids.length, sizeof(int64_t), kl_test_ios_cmp_int64);
  int64_t duplicate_ids = 0;
  for (int64_t i = 1; i < all_ids.length; i++) {
    if (all_ids.items[i] == all_ids.items[i - 1]) duplicate_ids += 1;
  }
  KL_TEST_EXPECT_EQ_INT(duplicate_ids, 0);
  KL_TEST_EXPECT_EQ_INT(s.next_sink_id, TOTAL_SINKS);
  kl_int64_list_destroy(&all_ids);

  // ---- no leaks: everything is back at zero, and the footprint is bounded
  // by the per-frame peak, not by the total number of sinks churned ----
  KL_IOStreamSinkFunnel* fa = kl_io_stream_get_funnel(&s, KL_TEST_IOS_TYPE_A);
  KL_IOStreamSinkFunnel* fb = kl_io_stream_get_funnel(&s, KL_TEST_IOS_TYPE_B);
  KL_TEST_EXPECT_EQ_INT(fa->sinks.taken_indices.length, 0);
  KL_TEST_EXPECT_EQ_INT(fb->sinks.taken_indices.length, 0);
  KL_TEST_EXPECT_EQ_INT(kl_test_ios_sink_ref_count(&s), 0);
  KL_TEST_EXPECT_EQ_INT(kl_test_ios_live_blobs(&s), 0);
  // pool: at most one growth step of slack above the peak
  KL_TEST_EXPECT_EXPR(fa->sinks.capacity < PEAK_LIVE_SINKS_PER_TYPE + 2 * s.config.sink_count_capacity);
  KL_TEST_EXPECT_EXPR(fb->sinks.capacity < PEAK_LIVE_SINKS_PER_TYPE + 2 * s.config.sink_count_capacity);
  // ref map: unset slots are reused within their chain, so the slot array is
  // bounded by the per-bucket peaks (which sum to at most a small multiple of
  // the global peak), never by the total number of ids ever inserted.
  KL_TEST_EXPECT_EXPR(s.sink_refs.slots.length <= s.sink_refs.capacity_pow2 + 2 * PEAK_LIVE_SINKS);
  KL_TEST_EXPECT_EXPR(s.sink_refs.slots.length < TOTAL_SINKS / 2);
  // blob allocator: tracked slots are recycled, so their count is bounded by
  // the peak number of chunks that can be queued at once (+ writers in flight)
  KL_TEST_EXPECT_EXPR(s.blob_allocator.allocations.length <= PEAK_LIVE_SINKS * KL_TEST_IOS_STRESS_QUEUE_LENGTH + 2);
  KL_TEST_EXPECT_EXPR(s.blob_allocator.allocations.n_reused > 0);

  // ---- performance (loose bounds; the numbers are printed for eyeballing) ----
  double per_create_us = create_ms * 1000.0 / (double)created;
  double per_close_us = close_ms * 1000.0 / (double)closed;
  printf("    [stress] %d sinks over %d frames (%d live/frame), %ld chunks read, %ld writes\n",
         (int)TOTAL_SINKS, (int)KL_TEST_IOS_STRESS_FRAMES, (int)PEAK_LIVE_SINKS,
         (long)chunks_read, (long)(writers[0].writes + writers[1].writes));
  printf("    [stress] total %.1fms, worst frame %.1fms, create %.2fus/sink, close %.2fus/sink (per-thread, under contention)\n",
         total_ms, frame_ms_max, per_create_us, per_close_us);
  // Generous so CI noise can't trip them; the printed numbers are the real signal.
  KL_TEST_EXPECT_EXPR(per_create_us < 2000.0);
  KL_TEST_EXPECT_EXPR(per_close_us < 2000.0);
  KL_TEST_EXPECT_EXPR(frame_ms_max < 2000.0);

  kl_io_stream_destroy(&s);
}
