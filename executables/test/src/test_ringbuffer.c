#include <test/tests.h>

/* ===================== f32_ringbuffer tests ===================== */

// Float comparisons round-trip through memcpy, so values are bit-exact; a tiny
// tolerance is plenty and documents intent.
#define RB_EPS 1e-6f

void test_f32_ringbuffer_init_destroy() {
  KL_f32_RingBuffer rb = {0};
  kl_f32_ringbuffer_init(&rb, (KL_RingBufferConfig){ .length = 8 });

  KL_TEST_ASSERT(rb.initialized == true, "initialized flag set after init");
  KL_TEST_EXPECT_EQ_INT((int)rb.length, 8);
  KL_TEST_EXPECT_EQ_INT((int)rb.config.length, 8);
  KL_TEST_ASSERT(rb.items != 0, "items buffer allocated by init");
  KL_TEST_EXPECT_EQ_INT((int)rb.cursor_read, 0);
  KL_TEST_EXPECT_EQ_INT((int)rb.cursor_write, 0);
  KL_TEST_ASSERT(((uintptr_t)rb.items % alignof(float)) == 0, "items aligned for float");

  kl_f32_ringbuffer_destroy(&rb);
  KL_TEST_ASSERT(rb.initialized == false, "initialized flag cleared after destroy");
  KL_TEST_ASSERT(rb.items == 0, "items buffer released after destroy");
  KL_TEST_EXPECT_EQ_INT((int)rb.length, 0);
}

void test_f32_ringbuffer_double_init_is_noop() {
  KL_f32_RingBuffer rb = {0};
  kl_f32_ringbuffer_init(&rb, (KL_RingBufferConfig){ .length = 4 });
  float* items_before = rb.items;

  // Re-initing an already-initialized ringbuffer must be a no-op: neither the
  // length nor the backing buffer changes.
  kl_f32_ringbuffer_init(&rb, (KL_RingBufferConfig){ .length = 9999 });
  KL_TEST_EXPECT_EQ_INT((int)rb.length, 4);
  KL_TEST_ASSERT(rb.items == items_before, "backing buffer unchanged by second init");

  kl_f32_ringbuffer_destroy(&rb);
}

void test_f32_ringbuffer_write_read_round_trip() {
  KL_f32_RingBuffer rb = {0};
  kl_f32_ringbuffer_init(&rb, (KL_RingBufferConfig){ .length = 8 });

  kl_f32_ringbuffer_write(&rb, 1.0f);
  kl_f32_ringbuffer_write(&rb, 2.0f);
  kl_f32_ringbuffer_write(&rb, 3.0f);

  // Reading is FIFO as long as the writer has not lapped the reader.
  KL_TEST_EXPECT_EQ_FLOAT(kl_f32_ringbuffer_read(&rb), 1.0f, RB_EPS);
  KL_TEST_EXPECT_EQ_FLOAT(kl_f32_ringbuffer_read(&rb), 2.0f, RB_EPS);
  KL_TEST_EXPECT_EQ_FLOAT(kl_f32_ringbuffer_read(&rb), 3.0f, RB_EPS);

  kl_f32_ringbuffer_destroy(&rb);
}

void test_f32_ringbuffer_write_advances_write_cursor() {
  KL_f32_RingBuffer rb = {0};
  kl_f32_ringbuffer_init(&rb, (KL_RingBufferConfig){ .length = 8 });

  for (int i = 0; i < 3; i++) {
    kl_f32_ringbuffer_write(&rb, (float)i);
    KL_TEST_EXPECT_EQ_INT((int)rb.cursor_write, i + 1);
  }
  // The reader has not moved.
  KL_TEST_EXPECT_EQ_INT((int)rb.cursor_read, 0);
  // Underlying slots hold the written values in order.
  for (int i = 0; i < 3; i++) KL_TEST_EXPECT_EQ_FLOAT(rb.items[i], (float)i, RB_EPS);

  kl_f32_ringbuffer_destroy(&rb);
}

void test_f32_ringbuffer_read_advances_read_cursor() {
  KL_f32_RingBuffer rb = {0};
  kl_f32_ringbuffer_init(&rb, (KL_RingBufferConfig){ .length = 8 });

  for (int i = 0; i < 4; i++) kl_f32_ringbuffer_write(&rb, (float)(i + 10));
  for (int i = 0; i < 4; i++) {
    KL_TEST_EXPECT_EQ_INT((int)rb.cursor_read, i);
    KL_TEST_EXPECT_EQ_FLOAT(kl_f32_ringbuffer_read(&rb), (float)(i + 10), RB_EPS);
  }
  KL_TEST_EXPECT_EQ_INT((int)rb.cursor_read, 4);

  kl_f32_ringbuffer_destroy(&rb);
}

void test_f32_ringbuffer_write_cursor_wraps() {
  // Writing more than `length` items wraps the write cursor modulo length and
  // overwrites the oldest slots.
  KL_f32_RingBuffer rb = {0};
  kl_f32_ringbuffer_init(&rb, (KL_RingBufferConfig){ .length = 4 });

  for (int i = 0; i < 4; i++) kl_f32_ringbuffer_write(&rb, (float)i); // fills, cursor -> 0
  KL_TEST_EXPECT_EQ_INT((int)rb.cursor_write, 0);

  kl_f32_ringbuffer_write(&rb, 99.0f); // overwrites slot 0
  KL_TEST_EXPECT_EQ_INT((int)rb.cursor_write, 1);
  KL_TEST_EXPECT_EQ_FLOAT(rb.items[0], 99.0f, RB_EPS);
  KL_TEST_EXPECT_EQ_FLOAT(rb.items[1], 1.0f, RB_EPS); // untouched

  kl_f32_ringbuffer_destroy(&rb);
}

void test_f32_ringbuffer_read_cursor_wraps() {
  KL_f32_RingBuffer rb = {0};
  kl_f32_ringbuffer_init(&rb, (KL_RingBufferConfig){ .length = 4 });

  for (int i = 0; i < 4; i++) kl_f32_ringbuffer_write(&rb, (float)i);
  for (int i = 0; i < 4; i++) kl_f32_ringbuffer_read(&rb); // cursor_read -> 0
  KL_TEST_EXPECT_EQ_INT((int)rb.cursor_read, 0);

  // Reading again wraps and re-reads slot 0 (still holds 0.0f).
  KL_TEST_EXPECT_EQ_FLOAT(kl_f32_ringbuffer_read(&rb), 0.0f, RB_EPS);
  KL_TEST_EXPECT_EQ_INT((int)rb.cursor_read, 1);

  kl_f32_ringbuffer_destroy(&rb);
}

void test_f32_ringbuffer_full_cycle_returns_to_origin() {
  // Writing then reading exactly `length` items leaves both cursors back at 0.
  KL_f32_RingBuffer rb = {0};
  enum { N = 6 };
  kl_f32_ringbuffer_init(&rb, (KL_RingBufferConfig){ .length = N });

  for (int i = 0; i < N; i++) kl_f32_ringbuffer_write(&rb, (float)(i * 2));
  KL_TEST_EXPECT_EQ_INT((int)rb.cursor_write, 0);

  for (int i = 0; i < N; i++) {
    KL_TEST_EXPECT_EQ_FLOAT(kl_f32_ringbuffer_read(&rb), (float)(i * 2), RB_EPS);
  }
  KL_TEST_EXPECT_EQ_INT((int)rb.cursor_read, 0);

  kl_f32_ringbuffer_destroy(&rb);
}

void test_f32_ringbuffer_overwrite_keeps_latest_in_slots() {
  // Classic ring usage: write 2*length items into a length-4 buffer. Each slot
  // ends up holding the most recent value written to its index.
  KL_f32_RingBuffer rb = {0};
  kl_f32_ringbuffer_init(&rb, (KL_RingBufferConfig){ .length = 4 });

  for (int i = 0; i < 10; i++) kl_f32_ringbuffer_write(&rb, (float)i);
  // 10 writes into 4 slots: cursor_write = 10 % 4 = 2.
  KL_TEST_EXPECT_EQ_INT((int)rb.cursor_write, 2);
  // slot i holds the last value v with v % 4 == i: [8, 9, 6, 7].
  KL_TEST_EXPECT_EQ_FLOAT(rb.items[0], 8.0f, RB_EPS);
  KL_TEST_EXPECT_EQ_FLOAT(rb.items[1], 9.0f, RB_EPS);
  KL_TEST_EXPECT_EQ_FLOAT(rb.items[2], 6.0f, RB_EPS);
  KL_TEST_EXPECT_EQ_FLOAT(rb.items[3], 7.0f, RB_EPS);

  kl_f32_ringbuffer_destroy(&rb);
}

void test_f32_ringbuffer_interleaved_read_write_is_fifo() {
  // As long as the writer stays ahead of the reader without lapping it, the
  // reader observes values in the order they were written.
  KL_f32_RingBuffer rb = {0};
  kl_f32_ringbuffer_init(&rb, (KL_RingBufferConfig){ .length = 8 });

  float next_write = 0.0f;
  float next_read = 0.0f;

  // Keep 2 items "in flight": write 2, then for each step write one + read one.
  kl_f32_ringbuffer_write(&rb, next_write++);
  kl_f32_ringbuffer_write(&rb, next_write++);
  for (int i = 0; i < 50; i++) {
    kl_f32_ringbuffer_write(&rb, next_write++);
    KL_TEST_EXPECT_EQ_FLOAT(kl_f32_ringbuffer_read(&rb), next_read, RB_EPS);
    next_read += 1.0f;
  }
  // Drain the 2 still in flight.
  KL_TEST_EXPECT_EQ_FLOAT(kl_f32_ringbuffer_read(&rb), next_read, RB_EPS);
  next_read += 1.0f;
  KL_TEST_EXPECT_EQ_FLOAT(kl_f32_ringbuffer_read(&rb), next_read, RB_EPS);

  kl_f32_ringbuffer_destroy(&rb);
}

void test_f32_ringbuffer_reset_zeros_and_rewinds() {
  KL_f32_RingBuffer rb = {0};
  kl_f32_ringbuffer_init(&rb, (KL_RingBufferConfig){ .length = 4 });

  for (int i = 0; i < 3; i++) kl_f32_ringbuffer_write(&rb, (float)(i + 1));
  kl_f32_ringbuffer_read(&rb); // move read cursor off zero too

  kl_f32_ringbuffer_reset(&rb);
  KL_TEST_EXPECT_EQ_INT((int)rb.cursor_read, 0);
  KL_TEST_EXPECT_EQ_INT((int)rb.cursor_write, 0);
  KL_TEST_ASSERT(rb.items != 0, "buffer retained after reset");
  // Every slot is zeroed, so a full lap of reads yields 0.
  for (int i = 0; i < 4; i++) {
    KL_TEST_EXPECT_EQ_FLOAT(kl_f32_ringbuffer_read(&rb), 0.0f, RB_EPS);
  }

  kl_f32_ringbuffer_destroy(&rb);
}

void test_f32_ringbuffer_reset_allows_reuse() {
  KL_f32_RingBuffer rb = {0};
  kl_f32_ringbuffer_init(&rb, (KL_RingBufferConfig){ .length = 4 });

  for (int i = 0; i < 4; i++) kl_f32_ringbuffer_write(&rb, (float)i);
  kl_f32_ringbuffer_reset(&rb);

  for (int i = 0; i < 4; i++) kl_f32_ringbuffer_write(&rb, (float)(100 + i));
  for (int i = 0; i < 4; i++) {
    KL_TEST_EXPECT_EQ_FLOAT(kl_f32_ringbuffer_read(&rb), (float)(100 + i), RB_EPS);
  }

  kl_f32_ringbuffer_destroy(&rb);
}

void test_f32_ringbuffer_length_one() {
  // Degenerate single-slot buffer: cursors never leave index 0 and each write
  // clobbers the previous value.
  KL_f32_RingBuffer rb = {0};
  kl_f32_ringbuffer_init(&rb, (KL_RingBufferConfig){ .length = 1 });

  kl_f32_ringbuffer_write(&rb, 7.0f);
  KL_TEST_EXPECT_EQ_INT((int)rb.cursor_write, 0);
  KL_TEST_EXPECT_EQ_FLOAT(kl_f32_ringbuffer_read(&rb), 7.0f, RB_EPS);
  KL_TEST_EXPECT_EQ_INT((int)rb.cursor_read, 0);

  kl_f32_ringbuffer_write(&rb, -3.5f);
  KL_TEST_EXPECT_EQ_FLOAT(kl_f32_ringbuffer_read(&rb), -3.5f, RB_EPS);

  kl_f32_ringbuffer_destroy(&rb);
}

void test_f32_ringbuffer_preserves_special_values() {
  KL_f32_RingBuffer rb = {0};
  float values[] = { 0.0f, -0.0f, -1.5f, 3.25f, 1e30f, -1e-30f, 42.0f };
  int n = (int)KL_ARRAY_LEN(values);
  kl_f32_ringbuffer_init(&rb, (KL_RingBufferConfig){ .length = 8 });

  for (int i = 0; i < n; i++) kl_f32_ringbuffer_write(&rb, values[i]);
  for (int i = 0; i < n; i++) {
    KL_TEST_ASSERT(kl_f32_ringbuffer_read(&rb) == values[i], "value round-trips exactly");
  }

  kl_f32_ringbuffer_destroy(&rb);
}

void test_f32_ringbuffer_stress_many_laps() {
  // Drive many laps around a small buffer keeping one item in flight; the
  // reader must always see the writer's previous value.
  KL_f32_RingBuffer rb = {0};
  kl_f32_ringbuffer_init(&rb, (KL_RingBufferConfig){ .length = 3 });

  enum { N = 10000 };
  kl_f32_ringbuffer_write(&rb, 0.0f);
  for (int i = 1; i < N; i++) {
    kl_f32_ringbuffer_write(&rb, (float)i);
    KL_TEST_EXPECT_EQ_FLOAT(kl_f32_ringbuffer_read(&rb), (float)(i - 1), RB_EPS);
  }
  KL_TEST_EXPECT_EQ_FLOAT(kl_f32_ringbuffer_read(&rb), (float)(N - 1), RB_EPS);
  // Cursors stayed in range the whole time.
  KL_TEST_ASSERT(rb.cursor_read >= 0 && rb.cursor_read < rb.length, "read cursor in range");
  KL_TEST_ASSERT(rb.cursor_write >= 0 && rb.cursor_write < rb.length, "write cursor in range");

  kl_f32_ringbuffer_destroy(&rb);
}

void test_f32_ringbuffer_with_arena_allocator() {
  // When a config allocator is supplied, init draws the backing buffer from it
  // and destroy must NOT call free() on arena-owned memory.
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 4096 });

  KL_f32_RingBuffer rb = {0};
  kl_f32_ringbuffer_init(&rb, (KL_RingBufferConfig){ .length = 16, .allocator = &a });
  KL_TEST_ASSERT(rb.items != 0, "items drawn from arena");
  KL_TEST_ASSERT(rb.config.allocator == &a, "config retains the allocator");
  KL_TEST_ASSERT(((uintptr_t)rb.items % alignof(float)) == 0, "arena items aligned for float");

  for (int i = 0; i < 16; i++) kl_f32_ringbuffer_write(&rb, (float)(i * 3));
  for (int i = 0; i < 16; i++) {
    KL_TEST_EXPECT_EQ_FLOAT(kl_f32_ringbuffer_read(&rb), (float)(i * 3), RB_EPS);
  }

  kl_f32_ringbuffer_destroy(&rb);
  KL_TEST_ASSERT(rb.initialized == false, "destroyed");

  // Allocator outlives the ringbuffer and is still healthy.
  uint8_t* p = kl_allocator_malloc(&a, 32);
  KL_TEST_ASSERT(p != 0, "allocator still usable after ringbuffer destroy");

  kl_allocator_destroy(&a);
}
