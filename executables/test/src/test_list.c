#include <test/tests.h>

/* ===================== int64_list tests ===================== */

void test_int64_list_init_destroy() {
  KL_int64_List list = {0};
  KL_TEST_EXPECT_EQ_INT(kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 }), 1);
  KL_TEST_ASSERT(list.initialized == true, "initialized flag set after init");
  KL_TEST_EXPECT_EQ_INT(list.length, 0);
  KL_TEST_EXPECT_EQ_INT(list.n_slots_allocated, 0);
  KL_TEST_EXPECT_EQ_INT(list.current_capacity, 4);
  KL_TEST_ASSERT(list.items == 0, "no items buffer before first push/grow");
  KL_TEST_ASSERT(list.alignment >= alignof(int64_t), "alignment at least alignof(T)");

  kl_int64_list_destroy(&list);
  KL_TEST_ASSERT(list.initialized == false, "initialized flag cleared after destroy");
  KL_TEST_ASSERT(list.items == 0, "items buffer released after destroy");
  KL_TEST_EXPECT_EQ_INT(list.length, 0);
  KL_TEST_EXPECT_EQ_INT(list.n_slots_allocated, 0);
  KL_TEST_EXPECT_EQ_INT(list.current_capacity, 0);
}

void test_int64_list_double_init_is_noop() {
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 8 });
  int64_t cap_before = list.current_capacity;
  // Re-initing an already-initialized list must be a no-op.
  KL_TEST_EXPECT_EQ_INT(kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 9999 }), 1);
  KL_TEST_EXPECT_EQ_INT(list.current_capacity, cap_before);
  kl_int64_list_destroy(&list);
}

void test_int64_list_push_basic() {
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });

  kl_int64_list_push(&list, 42);
  KL_TEST_EXPECT_EQ_INT(list.length, 1);
  KL_TEST_ASSERT(list.items != 0, "items buffer allocated on first push");
  KL_TEST_ASSERT(list.n_slots_allocated >= 1, "at least one slot allocated");
  KL_TEST_EXPECT_EQ_INT((int)list.items[0], 42);

  kl_int64_list_destroy(&list);
}

void test_int64_list_push_many_preserves_order() {
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });

  enum { N = 257 };
  for (int64_t i = 0; i < N; i++) {
    kl_int64_list_push(&list, i * 7 - 3);
  }
  KL_TEST_EXPECT_EQ_INT(list.length, N);
  for (int64_t i = 0; i < N; i++) {
    KL_TEST_EXPECT_EQ_INT((int)list.items[i], (int)(i * 7 - 3));
  }

  kl_int64_list_destroy(&list);
}

void test_int64_list_push_grows_capacity() {
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });

  // First 4 pushes trigger the initial grow (avail == 0 the very first push).
  for (int i = 0; i < 4; i++) kl_int64_list_push(&list, i);
  int64_t slots_after_first_round = list.n_slots_allocated;
  KL_TEST_ASSERT(slots_after_first_round >= 4, "first growth covers at least initial capacity");

  // Force a second growth round.
  for (int i = 0; i < (int)slots_after_first_round; i++) {
    kl_int64_list_push(&list, 1000 + i);
  }
  KL_TEST_ASSERT(list.n_slots_allocated > slots_after_first_round, "second growth extended buffer");
  KL_TEST_EXPECT_EQ_INT(list.length, 4 + (int)slots_after_first_round);

  // Earlier values are not clobbered by reallocation.
  for (int i = 0; i < 4; i++) KL_TEST_EXPECT_EQ_INT((int)list.items[i], i);
  for (int i = 0; i < (int)slots_after_first_round; i++) {
    KL_TEST_EXPECT_EQ_INT((int)list.items[4 + i], 1000 + i);
  }

  kl_int64_list_destroy(&list);
}

void test_int64_list_grow_increases_slots() {
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });

  kl_int64_list_grow(&list, 16);
  KL_TEST_EXPECT_EQ_INT(list.n_slots_allocated, 16);
  KL_TEST_ASSERT(list.items != 0, "items buffer present after grow");

  // Calling grow again is additive.
  kl_int64_list_grow(&list, 8);
  KL_TEST_EXPECT_EQ_INT(list.n_slots_allocated, 24);

  kl_int64_list_destroy(&list);
}

void test_int64_list_grow_zeros_new_slots() {
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });

  kl_int64_list_grow(&list, 32);
  for (int i = 0; i < 32; i++) KL_TEST_EXPECT_EQ_INT((int)list.items[i], 0);

  // Scribble, then grow again and confirm the new tail region is zeroed
  // without clobbering the old data.
  for (int i = 0; i < 32; i++) list.items[i] = 100 + i;
  kl_int64_list_grow(&list, 16);
  for (int i = 0; i < 32; i++) KL_TEST_EXPECT_EQ_INT((int)list.items[i], 100 + i);
  for (int i = 32; i < 48; i++) KL_TEST_EXPECT_EQ_INT((int)list.items[i], 0);

  kl_int64_list_destroy(&list);
}

void test_int64_list_pop_basic() {
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });
  kl_int64_list_push(&list, 10);
  kl_int64_list_push(&list, 20);
  kl_int64_list_push(&list, 30);

  int64_t out = 0;
  KL_TEST_EXPECT_EQ_INT(kl_int64_list_pop(&list, &out), 1);
  KL_TEST_EXPECT_EQ_INT((int)out, 30);
  KL_TEST_EXPECT_EQ_INT(list.length, 2);

  KL_TEST_EXPECT_EQ_INT(kl_int64_list_pop(&list, &out), 1);
  KL_TEST_EXPECT_EQ_INT((int)out, 20);
  KL_TEST_EXPECT_EQ_INT(list.length, 1);

  kl_int64_list_destroy(&list);
}

void test_int64_list_pop_lifo_order() {
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });
  for (int i = 0; i < 50; i++) kl_int64_list_push(&list, i);

  for (int i = 49; i >= 0; i--) {
    int64_t out = 0;
    KL_TEST_EXPECT_EQ_INT(kl_int64_list_pop(&list, &out), 1);
    KL_TEST_EXPECT_EQ_INT((int)out, i);
  }
  KL_TEST_EXPECT_EQ_INT(list.length, 0);

  kl_int64_list_destroy(&list);
}

void test_int64_list_pop_with_null_out() {
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });
  kl_int64_list_push(&list, 7);
  kl_int64_list_push(&list, 8);

  KL_TEST_EXPECT_EQ_INT(kl_int64_list_pop(&list, 0), 1);
  KL_TEST_EXPECT_EQ_INT(list.length, 1);
  KL_TEST_EXPECT_EQ_INT((int)list.items[0], 7);

  kl_int64_list_destroy(&list);
}

void test_int64_list_pop_empty_returns_zero() {
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });

  int64_t out = 999;
  KL_TEST_EXPECT_EQ_INT(kl_int64_list_pop(&list, &out), 0);
  KL_TEST_EXPECT_EQ_INT((int)out, 999); // out is left untouched
  KL_TEST_EXPECT_EQ_INT(list.length, 0);

  // Pop after pushing then draining returns 0 again.
  kl_int64_list_push(&list, 1);
  kl_int64_list_pop(&list, 0);
  KL_TEST_EXPECT_EQ_INT(kl_int64_list_pop(&list, &out), 0);

  kl_int64_list_destroy(&list);
}

void test_int64_list_pop_at_first() {
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });
  for (int i = 0; i < 5; i++) kl_int64_list_push(&list, i + 1); // [1,2,3,4,5]

  int64_t out = 0;
  KL_TEST_EXPECT_EQ_INT(kl_int64_list_pop_at(&list, 0, &out), 1);
  KL_TEST_EXPECT_EQ_INT((int)out, 1);
  KL_TEST_EXPECT_EQ_INT(list.length, 4);
  // Remaining: [2,3,4,5]
  for (int i = 0; i < 4; i++) KL_TEST_EXPECT_EQ_INT((int)list.items[i], i + 2);

  kl_int64_list_destroy(&list);
}

void test_int64_list_pop_at_middle() {
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });
  for (int i = 0; i < 6; i++) kl_int64_list_push(&list, i * 10); // [0,10,20,30,40,50]

  int64_t out = 0;
  KL_TEST_EXPECT_EQ_INT(kl_int64_list_pop_at(&list, 2, &out), 1);
  KL_TEST_EXPECT_EQ_INT((int)out, 20);
  KL_TEST_EXPECT_EQ_INT(list.length, 5);
  // Remaining: [0,10,30,40,50]
  int expected[] = { 0, 10, 30, 40, 50 };
  for (int i = 0; i < 5; i++) KL_TEST_EXPECT_EQ_INT((int)list.items[i], expected[i]);

  kl_int64_list_destroy(&list);
}

void test_int64_list_pop_at_last() {
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });
  for (int i = 0; i < 4; i++) kl_int64_list_push(&list, i); // [0,1,2,3]

  int64_t out = 0;
  KL_TEST_EXPECT_EQ_INT(kl_int64_list_pop_at(&list, 3, &out), 1);
  KL_TEST_EXPECT_EQ_INT((int)out, 3);
  KL_TEST_EXPECT_EQ_INT(list.length, 3);
  for (int i = 0; i < 3; i++) KL_TEST_EXPECT_EQ_INT((int)list.items[i], i);

  kl_int64_list_destroy(&list);
}

void test_int64_list_pop_at_empty_returns_zero() {
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });
  // Never pushed: items is null. Must return 0 without asserting.
  int64_t out = 999;
  KL_TEST_EXPECT_EQ_INT(kl_int64_list_pop_at(&list, 0, &out), 0);
  KL_TEST_EXPECT_EQ_INT((int)out, 999);
  kl_int64_list_destroy(&list);
}

void test_int64_list_push_start_basic() {
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });

  kl_int64_list_push_start(&list, 99);
  KL_TEST_EXPECT_EQ_INT(list.length, 1);
  KL_TEST_EXPECT_EQ_INT((int)list.items[0], 99);

  kl_int64_list_destroy(&list);
}

void test_int64_list_push_start_reverses_insertion_order() {
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });

  // Inserting 1,2,3 at the front produces [3,2,1].
  kl_int64_list_push_start(&list, 1);
  kl_int64_list_push_start(&list, 2);
  kl_int64_list_push_start(&list, 3);
  KL_TEST_EXPECT_EQ_INT(list.length, 3);
  KL_TEST_EXPECT_EQ_INT((int)list.items[0], 3);
  KL_TEST_EXPECT_EQ_INT((int)list.items[1], 2);
  KL_TEST_EXPECT_EQ_INT((int)list.items[2], 1);

  kl_int64_list_destroy(&list);
}

void test_int64_list_push_start_after_push() {
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });

  kl_int64_list_push(&list, 10);
  kl_int64_list_push(&list, 20);
  kl_int64_list_push(&list, 30); // [10,20,30]
  kl_int64_list_push_start(&list, 5); // [5,10,20,30]

  KL_TEST_EXPECT_EQ_INT(list.length, 4);
  int expected[] = { 5, 10, 20, 30 };
  for (int i = 0; i < 4; i++) KL_TEST_EXPECT_EQ_INT((int)list.items[i], expected[i]);

  kl_int64_list_destroy(&list);
}

void test_int64_list_push_start_grows_when_needed() {
  // Push_start must also trigger a grow when full, not just push.
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 2 });

  for (int i = 0; i < 20; i++) kl_int64_list_push_start(&list, i);
  KL_TEST_EXPECT_EQ_INT(list.length, 20);
  // After repeated push_start, items[0] is the most-recently inserted (19),
  // and items[length-1] is the first one inserted (0).
  KL_TEST_EXPECT_EQ_INT((int)list.items[0], 19);
  KL_TEST_EXPECT_EQ_INT((int)list.items[19], 0);
  for (int i = 0; i < 20; i++) {
    KL_TEST_EXPECT_EQ_INT((int)list.items[i], 19 - i);
  }

  kl_int64_list_destroy(&list);
}

void test_int64_list_includes_found() {
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });
  kl_int64_list_push(&list, 1);
  kl_int64_list_push(&list, 2);
  kl_int64_list_push(&list, 3);

  KL_TEST_ASSERT(kl_int64_list_includes(&list, 1) == true, "find first");
  KL_TEST_ASSERT(kl_int64_list_includes(&list, 2) == true, "find middle");
  KL_TEST_ASSERT(kl_int64_list_includes(&list, 3) == true, "find last");

  kl_int64_list_destroy(&list);
}

void test_int64_list_includes_not_found() {
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });
  kl_int64_list_push(&list, 1);
  kl_int64_list_push(&list, 2);
  kl_int64_list_push(&list, 3);

  KL_TEST_ASSERT(kl_int64_list_includes(&list, 0) == false, "0 not in list");
  KL_TEST_ASSERT(kl_int64_list_includes(&list, 4) == false, "4 not in list");
  KL_TEST_ASSERT(kl_int64_list_includes(&list, -1) == false, "negative not in list");

  kl_int64_list_destroy(&list);
}

void test_int64_list_includes_empty() {
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });
  KL_TEST_ASSERT(kl_int64_list_includes(&list, 0) == false, "empty list contains nothing");
  KL_TEST_ASSERT(kl_int64_list_includes(&list, 42) == false, "empty list contains nothing");
  kl_int64_list_destroy(&list);
}

void test_int64_list_includes_after_pop() {
  // Items past list->length are not considered, even if memory still holds them.
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });
  kl_int64_list_push(&list, 100);
  kl_int64_list_push(&list, 200);
  kl_int64_list_pop(&list, 0); // length=1, but items[1] still == 200

  KL_TEST_ASSERT(kl_int64_list_includes(&list, 100) == true, "live element found");
  KL_TEST_ASSERT(kl_int64_list_includes(&list, 200) == false, "popped element not found");

  kl_int64_list_destroy(&list);
}

void test_int64_list_reset_clears_length() {
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });
  for (int i = 0; i < 10; i++) kl_int64_list_push(&list, i);
  int64_t slots_before = list.n_slots_allocated;
  KL_TEST_ASSERT(slots_before > 0, "buffer was allocated");

  kl_int64_list_reset(&list);
  KL_TEST_EXPECT_EQ_INT(list.length, 0);
  KL_TEST_EXPECT_EQ_INT(list.n_slots_allocated, slots_before);
  KL_TEST_ASSERT(list.items != 0, "buffer retained after reset");

  kl_int64_list_destroy(&list);
}

void test_int64_list_reset_zeros_items() {
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });
  for (int i = 0; i < 8; i++) kl_int64_list_push(&list, 1000 + i);

  kl_int64_list_reset(&list);
  for (int64_t i = 0; i < list.n_slots_allocated; i++) {
    KL_TEST_EXPECT_EQ_INT((int)list.items[i], 0);
  }

  kl_int64_list_destroy(&list);
}

void test_int64_list_reset_allows_reuse() {
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });
  for (int i = 0; i < 5; i++) kl_int64_list_push(&list, i);
  kl_int64_list_reset(&list);

  for (int i = 0; i < 7; i++) kl_int64_list_push(&list, 100 + i);
  KL_TEST_EXPECT_EQ_INT(list.length, 7);
  for (int i = 0; i < 7; i++) KL_TEST_EXPECT_EQ_INT((int)list.items[i], 100 + i);

  kl_int64_list_destroy(&list);
}

void test_int64_list_reset_on_empty_is_safe() {
  // Reset before any push (items == 0) must not segfault.
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });
  kl_int64_list_reset(&list);
  KL_TEST_EXPECT_EQ_INT(list.length, 0);
  KL_TEST_ASSERT(list.items == 0, "no buffer was allocated yet");
  kl_int64_list_destroy(&list);
}

void test_int64_list_with_arena_allocator() {
  // The list's grow path uses realloc, which the arena only supports when
  // track_allocations == false.
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){
    .capacity = 4096,
    .track_allocations = false,
  });

  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4, .allocator = &a });

  for (int i = 0; i < 100; i++) kl_int64_list_push(&list, i * 3);
  KL_TEST_EXPECT_EQ_INT(list.length, 100);
  for (int i = 0; i < 100; i++) KL_TEST_EXPECT_EQ_INT((int)list.items[i], i * 3);

  // Destroying must not call free() on arena-owned memory; it must just
  // zero the bookkeeping. The allocator outlives the list.
  kl_int64_list_destroy(&list);
  KL_TEST_ASSERT(list.initialized == false, "destroyed");

  // Allocator is still healthy.
  uint8_t* p = kl_allocator_malloc(&a, 32);
  KL_TEST_ASSERT(p != 0, "allocator still usable after list destroy");

  kl_allocator_destroy(&a);
}

void test_int64_list_arena_destroy_frees_allocation() {
  // With a tracking arena, destroying the list must return its block to the
  // allocator's freed pool so the space is reusable.
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){
    .capacity = 4096,
    .tracked_allocations_capacity = 8,
    .track_allocations = true,
  });

  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 8, .allocator = &a });
  for (int i = 0; i < 8; i++) kl_int64_list_push(&list, i);
  KL_TEST_ASSERT(list.allocation.size > 0, "list has a tracked allocation");

  kl_int64_list_destroy(&list);
  KL_TEST_EXPECT_EQ_INT(a.allocations.n_freed, 1);
  KL_TEST_EXPECT_EQ_INT((int)list.allocation.size, 0);

  kl_allocator_destroy(&a);
}

void test_int64_list_arena_grow_path() {
  // Force the arena realloc-aligned path through repeated growth.
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 1024, .track_allocations = false });

  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 2, .allocator = &a });

  enum { N = 500 };
  for (int i = 0; i < N; i++) kl_int64_list_push(&list, (int64_t)i);
  KL_TEST_EXPECT_EQ_INT(list.length, N);
  for (int i = 0; i < N; i++) KL_TEST_EXPECT_EQ_INT((int)list.items[i], i);
  KL_TEST_ASSERT(((uintptr_t)list.items % alignof(int64_t)) == 0, "items aligned");

  kl_int64_list_destroy(&list);
  kl_allocator_destroy(&a);
}

void test_int64_list_push_pop_interleaved() {
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });

  // Simulate stack usage: push 0..9, pop 5, push 100..104, pop them all.
  for (int i = 0; i < 10; i++) kl_int64_list_push(&list, i);
  for (int i = 0; i < 5; i++) {
    int64_t out = 0;
    kl_int64_list_pop(&list, &out);
    KL_TEST_EXPECT_EQ_INT((int)out, 9 - i);
  }
  KL_TEST_EXPECT_EQ_INT(list.length, 5);

  for (int i = 0; i < 5; i++) kl_int64_list_push(&list, 100 + i);
  KL_TEST_EXPECT_EQ_INT(list.length, 10);
  // Top of stack is 104.
  int expected_tail[] = { 100, 101, 102, 103, 104 };
  for (int i = 0; i < 5; i++) KL_TEST_EXPECT_EQ_INT((int)list.items[5 + i], expected_tail[i]);

  kl_int64_list_destroy(&list);
}

void test_int64_list_handles_negative_and_extreme_values() {
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });

  int64_t values[] = { 0, -1, INT64_MIN, INT64_MAX, 42, -42 };
  int n = (int)KL_ARRAY_LEN(values);
  for (int i = 0; i < n; i++) kl_int64_list_push(&list, values[i]);

  for (int i = 0; i < n; i++) {
    KL_TEST_ASSERT(list.items[i] == values[i], "value round-trips through push");
    KL_TEST_ASSERT(kl_int64_list_includes(&list, values[i]) == true, "includes finds value");
  }

  kl_int64_list_destroy(&list);
}

void test_int64_list_stress_many_elements() {
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });

  enum { N = 10000 };
  for (int64_t i = 0; i < N; i++) kl_int64_list_push(&list, i * 2 + 1);
  KL_TEST_EXPECT_EQ_INT(list.length, N);
  for (int64_t i = 0; i < N; i++) KL_TEST_EXPECT_EQ_INT((int)list.items[i], (int)(i * 2 + 1));

  // Drain the whole thing.
  for (int64_t i = N - 1; i >= 0; i--) {
    int64_t out = 0;
    KL_TEST_EXPECT_EQ_INT(kl_int64_list_pop(&list, &out), 1);
    KL_TEST_EXPECT_EQ_INT((int)out, (int)(i * 2 + 1));
  }
  KL_TEST_EXPECT_EQ_INT(list.length, 0);

  kl_int64_list_destroy(&list);
}

void test_int64_list_concat_basic() {
  KL_int64_List a = {0};
  KL_int64_List b = {0};
  kl_int64_list_init(&a, (KL_ListConfig){ .capacity = 4 });
  kl_int64_list_init(&b, (KL_ListConfig){ .capacity = 4 });

  for (int i = 0; i < 3; i++) kl_int64_list_push(&a, i);       // [0,1,2]
  for (int i = 0; i < 3; i++) kl_int64_list_push(&b, 10 + i);  // [10,11,12]

  kl_int64_list_concat(&a, &b);
  KL_TEST_EXPECT_EQ_INT(a.length, 6);
  int expected[] = { 0, 1, 2, 10, 11, 12 };
  for (int i = 0; i < 6; i++) KL_TEST_EXPECT_EQ_INT((int)a.items[i], expected[i]);

  // The source list is untouched.
  KL_TEST_EXPECT_EQ_INT(b.length, 3);
  for (int i = 0; i < 3; i++) KL_TEST_EXPECT_EQ_INT((int)b.items[i], 10 + i);

  kl_int64_list_destroy(&a);
  kl_int64_list_destroy(&b);
}

void test_int64_list_concat_onto_empty() {
  KL_int64_List a = {0};
  KL_int64_List b = {0};
  kl_int64_list_init(&a, (KL_ListConfig){ .capacity = 2 });
  kl_int64_list_init(&b, (KL_ListConfig){ .capacity = 2 });

  // a has never been pushed to (items == 0); concat must grow it.
  for (int i = 0; i < 5; i++) kl_int64_list_push(&b, i * 100);

  kl_int64_list_concat(&a, &b);
  KL_TEST_EXPECT_EQ_INT(a.length, 5);
  KL_TEST_ASSERT(a.items != 0, "buffer allocated by concat");
  for (int i = 0; i < 5; i++) KL_TEST_EXPECT_EQ_INT((int)a.items[i], i * 100);

  kl_int64_list_destroy(&a);
  kl_int64_list_destroy(&b);
}

void test_int64_list_concat_empty_source_is_noop() {
  KL_int64_List a = {0};
  KL_int64_List b = {0};
  kl_int64_list_init(&a, (KL_ListConfig){ .capacity = 4 });
  kl_int64_list_init(&b, (KL_ListConfig){ .capacity = 4 });

  for (int i = 0; i < 3; i++) kl_int64_list_push(&a, i);

  // Empty (never-pushed) source.
  kl_int64_list_concat(&a, &b);
  KL_TEST_EXPECT_EQ_INT(a.length, 3);
  for (int i = 0; i < 3; i++) KL_TEST_EXPECT_EQ_INT((int)a.items[i], i);

  // Null source.
  kl_int64_list_concat(&a, 0);
  KL_TEST_EXPECT_EQ_INT(a.length, 3);

  kl_int64_list_destroy(&a);
  kl_int64_list_destroy(&b);
}

void test_int64_list_concat_large_forces_grow() {
  KL_int64_List a = {0};
  KL_int64_List b = {0};
  kl_int64_list_init(&a, (KL_ListConfig){ .capacity = 4 });
  kl_int64_list_init(&b, (KL_ListConfig){ .capacity = 4 });

  for (int i = 0; i < 10; i++) kl_int64_list_push(&a, i);
  enum { M = 1000 };
  for (int i = 0; i < M; i++) kl_int64_list_push(&b, 1000 + i);

  kl_int64_list_concat(&a, &b);
  KL_TEST_EXPECT_EQ_INT(a.length, 10 + M);
  KL_TEST_ASSERT(a.n_slots_allocated >= 10 + M, "buffer large enough after concat");
  for (int i = 0; i < 10; i++) KL_TEST_EXPECT_EQ_INT((int)a.items[i], i);
  for (int i = 0; i < M; i++) KL_TEST_EXPECT_EQ_INT((int)a.items[10 + i], 1000 + i);

  kl_int64_list_destroy(&a);
  kl_int64_list_destroy(&b);
}

void test_int64_list_pop_at_drains_from_front() {
  // Repeatedly popping index 0 should yield the original insertion order.
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });
  for (int i = 0; i < 20; i++) kl_int64_list_push(&list, i);

  for (int i = 0; i < 20; i++) {
    int64_t out = 0;
    KL_TEST_EXPECT_EQ_INT(kl_int64_list_pop_at(&list, 0, &out), 1);
    KL_TEST_EXPECT_EQ_INT((int)out, i);
  }
  KL_TEST_EXPECT_EQ_INT(list.length, 0);
  KL_TEST_EXPECT_EQ_INT(kl_int64_list_pop_at(&list, 0, 0), 0);

  kl_int64_list_destroy(&list);
}

void test_int64_list_splice_insert_middle() {
  // to_remove == 0: pure insertion, like JS splice(index, 0, item).
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });
  for (int i = 0; i < 5; i++) kl_int64_list_push(&list, i * 10);

  kl_int64_list_splice(&list, 2, 0, 99);
  KL_TEST_EXPECT_EQ_INT(list.length, 6);
  int64_t expect[] = {0, 10, 99, 20, 30, 40};
  for (int i = 0; i < 6; i++) KL_TEST_EXPECT_EQ_INT((int)list.items[i], (int)expect[i]);

  kl_int64_list_destroy(&list);
}

void test_int64_list_splice_insert_at_start_and_end() {
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });
  for (int i = 0; i < 3; i++) kl_int64_list_push(&list, i);

  kl_int64_list_splice(&list, 0, 0, -1);
  // index == length appends.
  kl_int64_list_splice(&list, list.length, 0, 100);
  KL_TEST_EXPECT_EQ_INT(list.length, 5);
  int64_t expect[] = {-1, 0, 1, 2, 100};
  for (int i = 0; i < 5; i++) KL_TEST_EXPECT_EQ_INT((int)list.items[i], (int)expect[i]);

  kl_int64_list_destroy(&list);
}

void test_int64_list_splice_replace_one() {
  // to_remove == 1: in-place replacement.
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });
  for (int i = 0; i < 5; i++) kl_int64_list_push(&list, i);

  kl_int64_list_splice(&list, 2, 1, 77);
  KL_TEST_EXPECT_EQ_INT(list.length, 5);
  int64_t expect[] = {0, 1, 77, 3, 4};
  for (int i = 0; i < 5; i++) KL_TEST_EXPECT_EQ_INT((int)list.items[i], (int)expect[i]);

  kl_int64_list_destroy(&list);
}

void test_int64_list_splice_remove_many() {
  // to_remove > 1 shrinks the list.
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });
  for (int i = 0; i < 8; i++) kl_int64_list_push(&list, i);

  kl_int64_list_splice(&list, 1, 5, 50);
  KL_TEST_EXPECT_EQ_INT(list.length, 4);
  int64_t expect[] = {0, 50, 6, 7};
  for (int i = 0; i < 4; i++) KL_TEST_EXPECT_EQ_INT((int)list.items[i], (int)expect[i]);

  kl_int64_list_destroy(&list);
}

void test_int64_list_splice_to_remove_clamped_to_tail() {
  // to_remove past the end removes only what exists, like JS.
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });
  for (int i = 0; i < 5; i++) kl_int64_list_push(&list, i);

  kl_int64_list_splice(&list, 3, 1000, 33);
  KL_TEST_EXPECT_EQ_INT(list.length, 4);
  int64_t expect[] = {0, 1, 2, 33};
  for (int i = 0; i < 4; i++) KL_TEST_EXPECT_EQ_INT((int)list.items[i], (int)expect[i]);

  kl_int64_list_destroy(&list);
}

void test_int64_list_splice_negative_to_remove_is_insert() {
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });
  for (int i = 0; i < 3; i++) kl_int64_list_push(&list, i);

  kl_int64_list_splice(&list, 1, -5, 11);
  KL_TEST_EXPECT_EQ_INT(list.length, 4);
  int64_t expect[] = {0, 11, 1, 2};
  for (int i = 0; i < 4; i++) KL_TEST_EXPECT_EQ_INT((int)list.items[i], (int)expect[i]);

  kl_int64_list_destroy(&list);
}

void test_int64_list_splice_into_empty() {
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });

  kl_int64_list_splice(&list, 0, 0, 42);
  KL_TEST_EXPECT_EQ_INT(list.length, 1);
  KL_TEST_ASSERT(list.items != 0, "items buffer allocated by splice into empty list");
  KL_TEST_EXPECT_EQ_INT((int)list.items[0], 42);

  kl_int64_list_destroy(&list);
}

void test_int64_list_splice_grows_when_full() {
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });
  for (int i = 0; i < 4; i++) kl_int64_list_push(&list, i);

  // Fill the buffer completely, then splice-insert until well past it.
  while (list.length < list.n_slots_allocated) kl_int64_list_push(&list, (int)list.length);
  int64_t full = list.length;
  for (int i = 0; i < 100; i++) {
    kl_int64_list_splice(&list, 1, 0, 1000 + i);
  }
  KL_TEST_EXPECT_EQ_INT(list.length, (int)full + 100);
  KL_TEST_ASSERT(list.n_slots_allocated >= list.length, "buffer grew to hold spliced items");
  KL_TEST_EXPECT_EQ_INT((int)list.items[0], 0);
  // Insertions at index 1 stack in reverse order.
  for (int i = 0; i < 100; i++) {
    KL_TEST_EXPECT_EQ_INT((int)list.items[1 + i], 1000 + (99 - i));
  }
  for (int i = 1; i < (int)full; i++) {
    KL_TEST_EXPECT_EQ_INT((int)list.items[100 + i], i);
  }

  kl_int64_list_destroy(&list);
}

void test_int64_list_splice_with_arena_allocator() {
  KL_Allocator alloc = {0};
  kl_allocator_init(&alloc, (KL_AllocatorConfig){
    .capacity = 4096,
    .track_allocations = false,
  });

  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4, .allocator = &alloc });
  for (int i = 0; i < 10; i++) kl_int64_list_push(&list, i);

  kl_int64_list_splice(&list, 5, 3, 55);
  KL_TEST_EXPECT_EQ_INT(list.length, 8);
  int64_t expect[] = {0, 1, 2, 3, 4, 55, 8, 9};
  for (int i = 0; i < 8; i++) KL_TEST_EXPECT_EQ_INT((int)list.items[i], (int)expect[i]);

  kl_int64_list_destroy(&list);
  kl_allocator_destroy(&alloc);
}
