#include <test/tests.h>
#include <kl/templates/unique_list/int64_unique_list.h>

/* ===================== int64_unique_list tests ===================== */

void test_int64_unique_list_init_destroy() {
  KL_int64_UniqueList list = {0};
  KL_TEST_EXPECT_EQ_INT(kl_int64_unique_list_init(&list, (KL_UniqueListConfig){ .capacity = 4 }), 1);
  KL_TEST_ASSERT(list.initialized == true, "initialized flag set after init");
  KL_TEST_EXPECT_EQ_INT(list._list.length, 0);
  KL_TEST_ASSERT(list._list.initialized == true, "underlying list initialized");
  KL_TEST_ASSERT(list.lookup.initialized == true, "lookup hashmap initialized");

  kl_int64_unique_list_destroy(&list);
  KL_TEST_ASSERT(list.initialized == false, "initialized flag cleared after destroy");
  KL_TEST_EXPECT_EQ_INT(list._list.length, 0);
}

void test_int64_unique_list_double_init_is_noop() {
  KL_int64_UniqueList list = {0};
  kl_int64_unique_list_init(&list, (KL_UniqueListConfig){ .capacity = 8 });
  int64_t cap_before = list.config.capacity;
  // Re-initing an already-initialized list must be a no-op.
  KL_TEST_EXPECT_EQ_INT(kl_int64_unique_list_init(&list, (KL_UniqueListConfig){ .capacity = 9999 }), 1);
  KL_TEST_EXPECT_EQ_INT(list.config.capacity, cap_before);
  kl_int64_unique_list_destroy(&list);
}

void test_int64_unique_list_push_basic() {
  KL_int64_UniqueList list = {0};
  kl_int64_unique_list_init(&list, (KL_UniqueListConfig){ .capacity = 4 });

  KL_TEST_ASSERT(kl_int64_unique_list_push(&list, 42) == true, "first push of a value succeeds");
  KL_TEST_EXPECT_EQ_INT(list._list.length, 1);
  KL_TEST_EXPECT_EQ_INT((int)list._list.items[0], 42);
  KL_TEST_ASSERT(kl_int64_unique_list_includes(&list, 42) == true, "pushed value is included");

  kl_int64_unique_list_destroy(&list);
}

void test_int64_unique_list_push_many_preserves_order() {
  KL_int64_UniqueList list = {0};
  kl_int64_unique_list_init(&list, (KL_UniqueListConfig){ .capacity = 4 });

  enum { N = 257 };
  for (int64_t i = 0; i < N; i++) {
    KL_TEST_ASSERT(kl_int64_unique_list_push(&list, i * 7 - 3) == true, "unique push succeeds");
  }
  KL_TEST_EXPECT_EQ_INT(list._list.length, N);
  for (int64_t i = 0; i < N; i++) {
    KL_TEST_EXPECT_EQ_INT((int)list._list.items[i], (int)(i * 7 - 3));
  }

  kl_int64_unique_list_destroy(&list);
}

void test_int64_unique_list_push_duplicate_rejected() {
  KL_int64_UniqueList list = {0};
  kl_int64_unique_list_init(&list, (KL_UniqueListConfig){ .capacity = 4 });

  KL_TEST_ASSERT(kl_int64_unique_list_push(&list, 10) == true, "first insert succeeds");
  KL_TEST_ASSERT(kl_int64_unique_list_push(&list, 10) == false, "duplicate insert is rejected");
  KL_TEST_ASSERT(kl_int64_unique_list_push(&list, 10) == false, "duplicate still rejected");
  KL_TEST_EXPECT_EQ_INT(list._list.length, 1);
  // The single stored copy is untouched.
  KL_TEST_EXPECT_EQ_INT((int)list._list.items[0], 10);

  kl_int64_unique_list_destroy(&list);
}

void test_int64_unique_list_push_duplicates_interleaved() {
  KL_int64_UniqueList list = {0};
  kl_int64_unique_list_init(&list, (KL_UniqueListConfig){ .capacity = 4 });

  // Insert 0..9 twice each; only the first of each pair should land.
  for (int round = 0; round < 2; round++) {
    for (int i = 0; i < 10; i++) {
      bool ok = kl_int64_unique_list_push(&list, i);
      KL_TEST_ASSERT(ok == (round == 0), "value lands only on its first appearance");
    }
  }
  KL_TEST_EXPECT_EQ_INT(list._list.length, 10);
  for (int i = 0; i < 10; i++) KL_TEST_EXPECT_EQ_INT((int)list._list.items[i], i);

  kl_int64_unique_list_destroy(&list);
}

void test_int64_unique_list_includes() {
  KL_int64_UniqueList list = {0};
  kl_int64_unique_list_init(&list, (KL_UniqueListConfig){ .capacity = 4 });
  kl_int64_unique_list_push(&list, 1);
  kl_int64_unique_list_push(&list, 2);
  kl_int64_unique_list_push(&list, 3);

  KL_TEST_ASSERT(kl_int64_unique_list_includes(&list, 1) == true, "find first");
  KL_TEST_ASSERT(kl_int64_unique_list_includes(&list, 2) == true, "find middle");
  KL_TEST_ASSERT(kl_int64_unique_list_includes(&list, 3) == true, "find last");
  KL_TEST_ASSERT(kl_int64_unique_list_includes(&list, 0) == false, "0 not present");
  KL_TEST_ASSERT(kl_int64_unique_list_includes(&list, 4) == false, "4 not present");
  KL_TEST_ASSERT(kl_int64_unique_list_includes(&list, -1) == false, "negative not present");

  kl_int64_unique_list_destroy(&list);
}

void test_int64_unique_list_includes_empty() {
  KL_int64_UniqueList list = {0};
  kl_int64_unique_list_init(&list, (KL_UniqueListConfig){ .capacity = 4 });
  KL_TEST_ASSERT(kl_int64_unique_list_includes(&list, 0) == false, "empty list contains nothing");
  KL_TEST_ASSERT(kl_int64_unique_list_includes(&list, 42) == false, "empty list contains nothing");
  kl_int64_unique_list_destroy(&list);
}

void test_int64_unique_list_index_of() {
  KL_int64_UniqueList list = {0};
  kl_int64_unique_list_init(&list, (KL_UniqueListConfig){ .capacity = 4 });
  kl_int64_unique_list_push(&list, 100);
  kl_int64_unique_list_push(&list, 200);
  kl_int64_unique_list_push(&list, 300);

  KL_TEST_EXPECT_EQ_INT((int)kl_int64_unique_list_index_of(&list, 100), 0);
  KL_TEST_EXPECT_EQ_INT((int)kl_int64_unique_list_index_of(&list, 200), 1);
  KL_TEST_EXPECT_EQ_INT((int)kl_int64_unique_list_index_of(&list, 300), 2);

  kl_int64_unique_list_destroy(&list);
}

void test_int64_unique_list_index_of_missing_returns_negative() {
  KL_int64_UniqueList list = {0};
  kl_int64_unique_list_init(&list, (KL_UniqueListConfig){ .capacity = 4 });
  kl_int64_unique_list_push(&list, 7);

  KL_TEST_EXPECT_EQ_INT((int)kl_int64_unique_list_index_of(&list, 7), 0);
  KL_TEST_EXPECT_EQ_INT((int)kl_int64_unique_list_index_of(&list, 8), -1);
  // Missing value on an otherwise-empty list also returns -1.
  KL_int64_UniqueList empty = {0};
  kl_int64_unique_list_init(&empty, (KL_UniqueListConfig){ .capacity = 4 });
  KL_TEST_EXPECT_EQ_INT((int)kl_int64_unique_list_index_of(&empty, 123), -1);

  kl_int64_unique_list_destroy(&list);
  kl_int64_unique_list_destroy(&empty);
}

void test_int64_unique_list_pop_basic() {
  KL_int64_UniqueList list = {0};
  kl_int64_unique_list_init(&list, (KL_UniqueListConfig){ .capacity = 4 });
  kl_int64_unique_list_push(&list, 10);
  kl_int64_unique_list_push(&list, 20);
  kl_int64_unique_list_push(&list, 30);

  int64_t out = 0;
  KL_TEST_ASSERT(kl_int64_unique_list_pop(&list, &out) == true, "pop succeeds");
  KL_TEST_EXPECT_EQ_INT((int)out, 30);
  KL_TEST_EXPECT_EQ_INT(list._list.length, 2);
  // The popped value is no longer a member.
  KL_TEST_ASSERT(kl_int64_unique_list_includes(&list, 30) == false, "popped value removed from lookup");
  KL_TEST_EXPECT_EQ_INT((int)kl_int64_unique_list_index_of(&list, 30), -1);

  kl_int64_unique_list_destroy(&list);
}

void test_int64_unique_list_pop_lifo_order() {
  KL_int64_UniqueList list = {0};
  kl_int64_unique_list_init(&list, (KL_UniqueListConfig){ .capacity = 4 });
  for (int i = 0; i < 50; i++) kl_int64_unique_list_push(&list, i);

  for (int i = 49; i >= 0; i--) {
    int64_t out = 0;
    KL_TEST_ASSERT(kl_int64_unique_list_pop(&list, &out) == true, "pop succeeds");
    KL_TEST_EXPECT_EQ_INT((int)out, i);
  }
  KL_TEST_EXPECT_EQ_INT(list._list.length, 0);

  kl_int64_unique_list_destroy(&list);
}

void test_int64_unique_list_pop_empty_returns_false() {
  KL_int64_UniqueList list = {0};
  kl_int64_unique_list_init(&list, (KL_UniqueListConfig){ .capacity = 4 });

  int64_t out = 999;
  KL_TEST_ASSERT(kl_int64_unique_list_pop(&list, &out) == false, "pop on empty returns false");
  KL_TEST_EXPECT_EQ_INT((int)out, 999); // out left untouched
  KL_TEST_EXPECT_EQ_INT(list._list.length, 0);

  kl_int64_unique_list_destroy(&list);
}

void test_int64_unique_list_pop_then_repush() {
  // A value that was popped must be insertable again (lookup was cleared).
  KL_int64_UniqueList list = {0};
  kl_int64_unique_list_init(&list, (KL_UniqueListConfig){ .capacity = 4 });
  kl_int64_unique_list_push(&list, 5);
  kl_int64_unique_list_push(&list, 6);

  int64_t out = 0;
  kl_int64_unique_list_pop(&list, &out); // removes 6
  KL_TEST_EXPECT_EQ_INT((int)out, 6);
  KL_TEST_ASSERT(kl_int64_unique_list_push(&list, 6) == true, "popped value can be re-pushed");
  KL_TEST_EXPECT_EQ_INT(list._list.length, 2);
  KL_TEST_EXPECT_EQ_INT((int)kl_int64_unique_list_index_of(&list, 6), 1);

  kl_int64_unique_list_destroy(&list);
}

void test_int64_unique_list_pop_at_middle_reindexes() {
  // Popping from the middle must shift remaining elements AND keep index_of
  // consistent with their new positions.
  KL_int64_UniqueList list = {0};
  kl_int64_unique_list_init(&list, (KL_UniqueListConfig){ .capacity = 4 });
  for (int i = 0; i < 6; i++) kl_int64_unique_list_push(&list, i * 10); // [0,10,20,30,40,50]

  int64_t out = 0;
  KL_TEST_ASSERT(kl_int64_unique_list_pop_at(&list, 2, &out) == true, "pop_at middle succeeds");
  KL_TEST_EXPECT_EQ_INT((int)out, 20);
  KL_TEST_EXPECT_EQ_INT(list._list.length, 5);

  // Remaining storage order: [0,10,30,40,50]
  int expected[] = { 0, 10, 30, 40, 50 };
  for (int i = 0; i < 5; i++) {
    KL_TEST_EXPECT_EQ_INT((int)list._list.items[i], expected[i]);
    // index_of must agree with the compacted storage position.
    KL_TEST_EXPECT_EQ_INT((int)kl_int64_unique_list_index_of(&list, expected[i]), i);
  }
  // The removed value is gone from the lookup.
  KL_TEST_ASSERT(kl_int64_unique_list_includes(&list, 20) == false, "removed value not included");
  KL_TEST_EXPECT_EQ_INT((int)kl_int64_unique_list_index_of(&list, 20), -1);

  kl_int64_unique_list_destroy(&list);
}

void test_int64_unique_list_pop_at_first_reindexes() {
  KL_int64_UniqueList list = {0};
  kl_int64_unique_list_init(&list, (KL_UniqueListConfig){ .capacity = 4 });
  for (int i = 0; i < 5; i++) kl_int64_unique_list_push(&list, i + 1); // [1,2,3,4,5]

  int64_t out = 0;
  KL_TEST_ASSERT(kl_int64_unique_list_pop_at(&list, 0, &out) == true, "pop_at first succeeds");
  KL_TEST_EXPECT_EQ_INT((int)out, 1);
  KL_TEST_EXPECT_EQ_INT(list._list.length, 4);
  // Remaining: [2,3,4,5], each index_of reflecting its new slot.
  for (int i = 0; i < 4; i++) {
    KL_TEST_EXPECT_EQ_INT((int)list._list.items[i], i + 2);
    KL_TEST_EXPECT_EQ_INT((int)kl_int64_unique_list_index_of(&list, i + 2), i);
  }
  KL_TEST_ASSERT(kl_int64_unique_list_includes(&list, 1) == false, "first value removed");

  kl_int64_unique_list_destroy(&list);
}

void test_int64_unique_list_pop_at_last_keeps_others() {
  KL_int64_UniqueList list = {0};
  kl_int64_unique_list_init(&list, (KL_UniqueListConfig){ .capacity = 4 });
  for (int i = 0; i < 4; i++) kl_int64_unique_list_push(&list, i); // [0,1,2,3]

  int64_t out = 0;
  KL_TEST_ASSERT(kl_int64_unique_list_pop_at(&list, 3, &out) == true, "pop_at last succeeds");
  KL_TEST_EXPECT_EQ_INT((int)out, 3);
  KL_TEST_EXPECT_EQ_INT(list._list.length, 3);
  for (int i = 0; i < 3; i++) {
    KL_TEST_EXPECT_EQ_INT((int)list._list.items[i], i);
    KL_TEST_EXPECT_EQ_INT((int)kl_int64_unique_list_index_of(&list, i), i);
  }

  kl_int64_unique_list_destroy(&list);
}

void test_int64_unique_list_pop_at_out_of_range_returns_false() {
  KL_int64_UniqueList list = {0};
  kl_int64_unique_list_init(&list, (KL_UniqueListConfig){ .capacity = 4 });
  kl_int64_unique_list_push(&list, 11);
  kl_int64_unique_list_push(&list, 22);

  int64_t out = 777;
  KL_TEST_ASSERT(kl_int64_unique_list_pop_at(&list, -1, &out) == false, "negative index rejected");
  KL_TEST_ASSERT(kl_int64_unique_list_pop_at(&list, 2, &out) == false, "index == length rejected");
  KL_TEST_ASSERT(kl_int64_unique_list_pop_at(&list, 99, &out) == false, "far-out index rejected");
  KL_TEST_EXPECT_EQ_INT((int)out, 777); // untouched
  KL_TEST_EXPECT_EQ_INT(list._list.length, 2);

  kl_int64_unique_list_destroy(&list);
}

void test_int64_unique_list_pop_at_empty_returns_false() {
  KL_int64_UniqueList list = {0};
  kl_int64_unique_list_init(&list, (KL_UniqueListConfig){ .capacity = 4 });
  // Never pushed: underlying items is null. Must return false without asserting.
  int64_t out = 999;
  KL_TEST_ASSERT(kl_int64_unique_list_pop_at(&list, 0, &out) == false, "pop_at on empty returns false");
  KL_TEST_EXPECT_EQ_INT((int)out, 999);
  kl_int64_unique_list_destroy(&list);
}

void test_int64_unique_list_pop_at_drains_from_front() {
  // Repeatedly popping index 0 yields original insertion order, and the
  // lookup stays consistent the whole way down.
  KL_int64_UniqueList list = {0};
  kl_int64_unique_list_init(&list, (KL_UniqueListConfig){ .capacity = 4 });
  for (int i = 0; i < 20; i++) kl_int64_unique_list_push(&list, i);

  for (int i = 0; i < 20; i++) {
    int64_t out = 0;
    KL_TEST_ASSERT(kl_int64_unique_list_pop_at(&list, 0, &out) == true, "pop_at front succeeds");
    KL_TEST_EXPECT_EQ_INT((int)out, i);
    KL_TEST_ASSERT(kl_int64_unique_list_includes(&list, i) == false, "drained value removed");
    // The new front is the next value, sitting at index 0.
    if (i + 1 < 20) {
      KL_TEST_EXPECT_EQ_INT((int)kl_int64_unique_list_index_of(&list, i + 1), 0);
    }
  }
  KL_TEST_EXPECT_EQ_INT(list._list.length, 0);
  KL_TEST_ASSERT(kl_int64_unique_list_pop_at(&list, 0, 0) == false, "empty after drain");

  kl_int64_unique_list_destroy(&list);
}

void test_int64_unique_list_grow_increases_slots() {
  KL_int64_UniqueList list = {0};
  kl_int64_unique_list_init(&list, (KL_UniqueListConfig){ .capacity = 4 });

  kl_int64_unique_list_grow(&list, 16);
  KL_TEST_EXPECT_EQ_INT(list._list.n_slots_allocated, 16);
  KL_TEST_ASSERT(list._list.items != 0, "items buffer present after grow");

  // grow is additive, and existing membership survives a grow.
  kl_int64_unique_list_push(&list, 123);
  kl_int64_unique_list_grow(&list, 8);
  KL_TEST_EXPECT_EQ_INT(list._list.n_slots_allocated, 24);
  KL_TEST_ASSERT(kl_int64_unique_list_includes(&list, 123) == true, "membership survives grow");

  kl_int64_unique_list_destroy(&list);
}

void test_int64_unique_list_reset_clears_membership() {
  KL_int64_UniqueList list = {0};
  kl_int64_unique_list_init(&list, (KL_UniqueListConfig){ .capacity = 4 });
  for (int i = 0; i < 10; i++) kl_int64_unique_list_push(&list, i);

  kl_int64_unique_list_reset(&list);
  KL_TEST_EXPECT_EQ_INT(list._list.length, 0);
  for (int i = 0; i < 10; i++) {
    KL_TEST_ASSERT(kl_int64_unique_list_includes(&list, i) == false, "reset clears membership");
    KL_TEST_EXPECT_EQ_INT((int)kl_int64_unique_list_index_of(&list, i), -1);
  }

  kl_int64_unique_list_destroy(&list);
}

void test_int64_unique_list_reset_allows_reuse() {
  KL_int64_UniqueList list = {0};
  kl_int64_unique_list_init(&list, (KL_UniqueListConfig){ .capacity = 4 });
  for (int i = 0; i < 5; i++) kl_int64_unique_list_push(&list, i);
  kl_int64_unique_list_reset(&list);

  // After reset, previously-seen values are insertable again with fresh indices.
  for (int i = 0; i < 7; i++) {
    KL_TEST_ASSERT(kl_int64_unique_list_push(&list, 100 + i) == true, "post-reset push succeeds");
  }
  KL_TEST_EXPECT_EQ_INT(list._list.length, 7);
  for (int i = 0; i < 7; i++) {
    KL_TEST_EXPECT_EQ_INT((int)list._list.items[i], 100 + i);
    KL_TEST_EXPECT_EQ_INT((int)kl_int64_unique_list_index_of(&list, 100 + i), i);
  }

  kl_int64_unique_list_destroy(&list);
}

void test_int64_unique_list_handles_negative_and_extreme_values() {
  KL_int64_UniqueList list = {0};
  kl_int64_unique_list_init(&list, (KL_UniqueListConfig){ .capacity = 4 });

  int64_t values[] = { 0, -1, INT64_MIN, INT64_MAX, 42, -42 };
  int n = (int)KL_ARRAY_LEN(values);
  for (int i = 0; i < n; i++) {
    KL_TEST_ASSERT(kl_int64_unique_list_push(&list, values[i]) == true, "extreme value pushes");
  }
  for (int i = 0; i < n; i++) {
    KL_TEST_ASSERT(list._list.items[i] == values[i], "value round-trips through push");
    KL_TEST_ASSERT(kl_int64_unique_list_includes(&list, values[i]) == true, "includes finds value");
    KL_TEST_EXPECT_EQ_INT((int)kl_int64_unique_list_index_of(&list, values[i]), i);
    // Each is a duplicate now and must be rejected.
    KL_TEST_ASSERT(kl_int64_unique_list_push(&list, values[i]) == false, "extreme value dedups");
  }
  KL_TEST_EXPECT_EQ_INT(list._list.length, n);

  kl_int64_unique_list_destroy(&list);
}

void test_int64_unique_list_with_arena_allocator() {
  // Grow paths use realloc, which the arena only supports when
  // track_allocations == false.
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){
    .capacity = 8192,
    .track_allocations = false,
  });

  KL_int64_UniqueList list = {0};
  kl_int64_unique_list_init(&list, (KL_UniqueListConfig){ .capacity = 4, .allocator = &a });

  enum { N = 300 };
  for (int i = 0; i < N; i++) {
    KL_TEST_ASSERT(kl_int64_unique_list_push(&list, i) == true, "arena-backed push succeeds");
  }
  // Re-pushing the same range is fully deduped.
  for (int i = 0; i < N; i++) {
    KL_TEST_ASSERT(kl_int64_unique_list_push(&list, i) == false, "arena-backed dedup works");
  }
  KL_TEST_EXPECT_EQ_INT(list._list.length, N);
  for (int i = 0; i < N; i++) KL_TEST_EXPECT_EQ_INT((int)kl_int64_unique_list_index_of(&list, i), i);

  kl_int64_unique_list_destroy(&list);
  KL_TEST_ASSERT(list.initialized == false, "destroyed");

  // Allocator outlives the list and is still healthy.
  uint8_t* p = kl_allocator_malloc(&a, 32);
  KL_TEST_ASSERT(p != 0, "allocator still usable after destroy");
  kl_allocator_destroy(&a);
}

void test_int64_unique_list_stress_dedup() {
  KL_int64_UniqueList list = {0};
  kl_int64_unique_list_init(&list, (KL_UniqueListConfig){ .capacity = 4 });

  enum { N = 5000 };
  // Push each value modulo a small range many times; only the first N_UNIQUE land.
  enum { N_UNIQUE = 256 };
  int64_t accepted = 0;
  for (int i = 0; i < N; i++) {
    if (kl_int64_unique_list_push(&list, i % N_UNIQUE)) accepted++;
  }
  KL_TEST_EXPECT_EQ_INT((int)accepted, N_UNIQUE);
  KL_TEST_EXPECT_EQ_INT(list._list.length, N_UNIQUE);
  for (int i = 0; i < N_UNIQUE; i++) {
    KL_TEST_ASSERT(kl_int64_unique_list_includes(&list, i) == true, "every unique value present");
    KL_TEST_EXPECT_EQ_INT((int)kl_int64_unique_list_index_of(&list, i), i);
  }

  kl_int64_unique_list_destroy(&list);
}
