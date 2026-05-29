#include <test/tests.h>

// ---------- binary_sparse_set ----------

typedef struct {
  KL_Allocator a;
  KL_BinarySparseSet set;
} KL_TestBSS;

static void kl_test_bss_setup(KL_TestBSS* fx, int64_t element_size, int64_t element_alignment,
                              int64_t sparse_capacity, int64_t dense_capacity) {
  // Realloc is exercised on every grow, so we keep tracking off (matches the
  // arena pattern used by the list tests).
  kl_allocator_init(&fx->a, (KL_AllocatorConfig){
    .capacity = 64 * 1024,
    .track_allocations = false,
  });
  fx->set = (KL_BinarySparseSet){0};
  kl_binary_sparseset_init(&fx->set, (KL_BinarySparseSetConfig){
    .sparse_capacity = sparse_capacity,
    .dense_capacity = dense_capacity,
    .element_size = element_size,
    .element_alignment = element_alignment,
    .allocator = &fx->a,
  });
}

static void kl_test_bss_teardown(KL_TestBSS* fx) {
  kl_binary_sparseset_destroy(&fx->set);
  kl_allocator_destroy(&fx->a);
}

void test_binary_sparseset_init_destroy() {
  KL_TestBSS fx = {0};
  kl_test_bss_setup(&fx, sizeof(int64_t), _Alignof(int64_t), 16, 8);

  KL_TEST_ASSERT(fx.set.initialized == true, "initialized after init");
  KL_TEST_EXPECT_EQ_INT((int)fx.set.length, 0);
  KL_TEST_EXPECT_EQ_INT((int)fx.set.sparse_capacity, 16);
  KL_TEST_EXPECT_EQ_INT((int)fx.set.dense_capacity, 8);
  KL_TEST_EXPECT_EQ_INT((int)fx.set.stride, (int)sizeof(int64_t));
  KL_TEST_EXPECT_EQ_INT((int)fx.set.alignment, (int)_Alignof(int64_t));
  KL_TEST_ASSERT(fx.set.sparse != 0, "sparse buffer allocated");
  KL_TEST_ASSERT(fx.set.indices != 0, "indices buffer allocated");
  KL_TEST_ASSERT(fx.set.values != 0, "values buffer allocated");
  // Sparse must be initialized to -1 (0xFF byte pattern fills int64_t lanes with -1).
  for (int64_t i = 0; i < fx.set.sparse_capacity; i++) {
    KL_TEST_ASSERT(fx.set.sparse[i] == -1, "sparse slot defaults to -1");
  }

  kl_binary_sparseset_destroy(&fx.set);
  KL_TEST_ASSERT(fx.set.initialized == false, "initialized cleared after destroy");
  KL_TEST_ASSERT(fx.set.sparse == 0, "sparse pointer cleared");
  KL_TEST_ASSERT(fx.set.indices == 0, "indices pointer cleared");
  KL_TEST_ASSERT(fx.set.values == 0, "values pointer cleared");
  KL_TEST_EXPECT_EQ_INT((int)fx.set.length, 0);
  KL_TEST_EXPECT_EQ_INT((int)fx.set.sparse_capacity, 0);
  KL_TEST_EXPECT_EQ_INT((int)fx.set.dense_capacity, 0);

  kl_allocator_destroy(&fx.a);
}

void test_binary_sparseset_double_init_is_noop() {
  KL_TestBSS fx = {0};
  kl_test_bss_setup(&fx, sizeof(int32_t), _Alignof(int32_t), 8, 4);

  int64_t sparse_cap_before = fx.set.sparse_capacity;
  int64_t dense_cap_before = fx.set.dense_capacity;
  int64_t stride_before = fx.set.stride;

  // A second init on an initialized set must short-circuit.
  kl_binary_sparseset_init(&fx.set, (KL_BinarySparseSetConfig){
    .sparse_capacity = 9999,
    .dense_capacity = 9999,
    .element_size = 4096,
    .element_alignment = 64,
    .allocator = &fx.a,
  });
  KL_TEST_EXPECT_EQ_INT((int)fx.set.sparse_capacity, (int)sparse_cap_before);
  KL_TEST_EXPECT_EQ_INT((int)fx.set.dense_capacity, (int)dense_cap_before);
  KL_TEST_EXPECT_EQ_INT((int)fx.set.stride, (int)stride_before);

  kl_test_bss_teardown(&fx);
}

void test_binary_sparseset_set_get_basic() {
  KL_TestBSS fx = {0};
  kl_test_bss_setup(&fx, sizeof(int64_t), _Alignof(int64_t), 16, 8);

  int64_t v = 12345;
  void* p = kl_binary_sparseset_set(&fx.set, 3, &v);
  KL_TEST_ASSERT(p != 0, "set returns non-null pointer");
  KL_TEST_EXPECT_EQ_INT((int)fx.set.length, 1);

  int64_t* got = (int64_t*)kl_binary_sparseset_get(&fx.set, 3);
  KL_TEST_ASSERT(got != 0, "get returns non-null for inserted id");
  KL_TEST_ASSERT(*got == 12345, "value round-trips");

  kl_test_bss_teardown(&fx);
}

void test_binary_sparseset_get_missing_returns_null() {
  KL_TestBSS fx = {0};
  kl_test_bss_setup(&fx, sizeof(int64_t), _Alignof(int64_t), 16, 8);

  KL_TEST_ASSERT(kl_binary_sparseset_get(&fx.set, 0) == 0, "get on empty returns null");
  KL_TEST_ASSERT(kl_binary_sparseset_get(&fx.set, 5) == 0, "get unset id returns null");

  int64_t v = 1;
  kl_binary_sparseset_set(&fx.set, 7, &v);
  KL_TEST_ASSERT(kl_binary_sparseset_get(&fx.set, 6) == 0, "neighbouring id still null");

  kl_test_bss_teardown(&fx);
}

void test_binary_sparseset_set_null_data_zero_inits() {
  KL_TestBSS fx = {0};
  kl_test_bss_setup(&fx, sizeof(int64_t), _Alignof(int64_t), 16, 8);

  // Pre-scribble the dense buffer to make sure zero-init actually writes zeros.
  memset(fx.set.values, 0xAA, (size_t)(fx.set.dense_capacity * fx.set.stride));

  void* p = kl_binary_sparseset_set(&fx.set, 4, 0);
  KL_TEST_ASSERT(p != 0, "set with NULL data returns non-null");
  int64_t* got = (int64_t*)kl_binary_sparseset_get(&fx.set, 4);
  KL_TEST_ASSERT(got != 0, "get returns inserted slot");
  KL_TEST_ASSERT(*got == 0, "NULL data zero-inits the slot");

  // Overwriting an existing slot with NULL also zeros it.
  int64_t v = 0x1234567890abcdefLL;
  kl_binary_sparseset_set(&fx.set, 4, &v);
  KL_TEST_ASSERT(*(int64_t*)kl_binary_sparseset_get(&fx.set, 4) == v, "overwrite stored");

  kl_binary_sparseset_set(&fx.set, 4, 0);
  KL_TEST_ASSERT(*(int64_t*)kl_binary_sparseset_get(&fx.set, 4) == 0, "NULL overwrite zeros");
  KL_TEST_EXPECT_EQ_INT((int)fx.set.length, 1);

  kl_test_bss_teardown(&fx);
}

void test_binary_sparseset_set_overwrite_keeps_length() {
  KL_TestBSS fx = {0};
  kl_test_bss_setup(&fx, sizeof(int64_t), _Alignof(int64_t), 16, 8);

  int64_t v = 1;
  kl_binary_sparseset_set(&fx.set, 5, &v);
  KL_TEST_EXPECT_EQ_INT((int)fx.set.length, 1);

  for (int i = 0; i < 10; i++) {
    int64_t w = 100 + i;
    void* p = kl_binary_sparseset_set(&fx.set, 5, &w);
    KL_TEST_ASSERT(p != 0, "overwrite returns non-null");
    KL_TEST_EXPECT_EQ_INT((int)fx.set.length, 1);
    KL_TEST_ASSERT(*(int64_t*)kl_binary_sparseset_get(&fx.set, 5) == w, "overwrite value visible");
  }

  kl_test_bss_teardown(&fx);
}

void test_binary_sparseset_set_returns_pointer_into_values() {
  KL_TestBSS fx = {0};
  kl_test_bss_setup(&fx, sizeof(int64_t), _Alignof(int64_t), 16, 8);

  int64_t v = 99;
  void* p1 = kl_binary_sparseset_set(&fx.set, 0, &v);
  void* p2 = kl_binary_sparseset_get(&fx.set, 0);
  KL_TEST_ASSERT(p1 == p2, "set and get return the same pointer");
  KL_TEST_ASSERT(p1 == (void*)fx.set.values, "first insert lives at dense index 0");

  v = 100;
  void* p3 = kl_binary_sparseset_set(&fx.set, 9, &v);
  KL_TEST_ASSERT(p3 == (void*)(fx.set.values + 1 * fx.set.stride), "second insert lives at dense index 1");

  kl_test_bss_teardown(&fx);
}

void test_binary_sparseset_has_before_and_after_set() {
  KL_TestBSS fx = {0};
  kl_test_bss_setup(&fx, sizeof(int64_t), _Alignof(int64_t), 16, 8);

  KL_TEST_ASSERT(!kl_binary_sparseset_has(&fx.set, 4), "has=false before set");
  int64_t v = 7;
  kl_binary_sparseset_set(&fx.set, 4, &v);
  KL_TEST_ASSERT(kl_binary_sparseset_has(&fx.set, 4), "has=true after set");
  KL_TEST_ASSERT(!kl_binary_sparseset_has(&fx.set, 3), "has=false for neighbour");

  kl_binary_sparseset_remove(&fx.set, 4);
  KL_TEST_ASSERT(!kl_binary_sparseset_has(&fx.set, 4), "has=false after remove");

  kl_test_bss_teardown(&fx);
}

void test_binary_sparseset_has_out_of_range_returns_false() {
  KL_TestBSS fx = {0};
  kl_test_bss_setup(&fx, sizeof(int64_t), _Alignof(int64_t), 16, 8);

  KL_TEST_ASSERT(!kl_binary_sparseset_has(&fx.set, -1), "negative id => false");
  KL_TEST_ASSERT(!kl_binary_sparseset_has(&fx.set, fx.set.sparse_capacity), "id == capacity => false");
  KL_TEST_ASSERT(!kl_binary_sparseset_has(&fx.set, 100000), "huge id => false");

  kl_test_bss_teardown(&fx);
}

void test_binary_sparseset_get_oob_returns_null() {
  KL_TestBSS fx = {0};
  kl_test_bss_setup(&fx, sizeof(int64_t), _Alignof(int64_t), 16, 8);

  KL_TEST_ASSERT(kl_binary_sparseset_get(&fx.set, -1) == 0, "negative id => null");
  KL_TEST_ASSERT(kl_binary_sparseset_get(&fx.set, fx.set.sparse_capacity) == 0, "id == capacity => null");

  kl_test_bss_teardown(&fx);
}

void test_binary_sparseset_remove_missing_returns_false() {
  KL_TestBSS fx = {0};
  kl_test_bss_setup(&fx, sizeof(int64_t), _Alignof(int64_t), 16, 8);

  KL_TEST_ASSERT(!kl_binary_sparseset_remove(&fx.set, 3), "remove on empty => false");
  KL_TEST_EXPECT_EQ_INT((int)fx.set.length, 0);

  int64_t v = 1;
  kl_binary_sparseset_set(&fx.set, 5, &v);
  KL_TEST_ASSERT(!kl_binary_sparseset_remove(&fx.set, 6), "remove non-existent id => false");
  KL_TEST_EXPECT_EQ_INT((int)fx.set.length, 1);
  KL_TEST_ASSERT(kl_binary_sparseset_has(&fx.set, 5), "untouched id still present");

  // Out-of-range removes are also false (kept inside the has() guard).
  KL_TEST_ASSERT(!kl_binary_sparseset_remove(&fx.set, -1), "negative id => false");
  KL_TEST_ASSERT(!kl_binary_sparseset_remove(&fx.set, 999), "OOB id => false");

  kl_test_bss_teardown(&fx);
}

void test_binary_sparseset_remove_last_element_no_swap() {
  KL_TestBSS fx = {0};
  kl_test_bss_setup(&fx, sizeof(int64_t), _Alignof(int64_t), 16, 8);

  int64_t a = 10, b = 20, c = 30;
  kl_binary_sparseset_set(&fx.set, 1, &a);
  kl_binary_sparseset_set(&fx.set, 2, &b);
  kl_binary_sparseset_set(&fx.set, 3, &c);
  KL_TEST_EXPECT_EQ_INT((int)fx.set.length, 3);

  // Remove the dense-tail element (id 3 was inserted last).
  KL_TEST_ASSERT(kl_binary_sparseset_remove(&fx.set, 3), "remove returns true");
  KL_TEST_EXPECT_EQ_INT((int)fx.set.length, 2);
  KL_TEST_ASSERT(!kl_binary_sparseset_has(&fx.set, 3), "removed id is gone");
  // Surviving elements untouched.
  KL_TEST_ASSERT(*(int64_t*)kl_binary_sparseset_get(&fx.set, 1) == 10, "a survives");
  KL_TEST_ASSERT(*(int64_t*)kl_binary_sparseset_get(&fx.set, 2) == 20, "b survives");
  // No swap happened: indices array still maps positions 0,1 to ids 1,2.
  KL_TEST_EXPECT_EQ_INT((int)fx.set.indices[0], 1);
  KL_TEST_EXPECT_EQ_INT((int)fx.set.indices[1], 2);

  kl_test_bss_teardown(&fx);
}

void test_binary_sparseset_remove_middle_swaps_with_last() {
  KL_TestBSS fx = {0};
  kl_test_bss_setup(&fx, sizeof(int64_t), _Alignof(int64_t), 16, 8);

  // Insert ids 10,11,12,13 -> dense positions 0,1,2,3.
  for (int64_t i = 0; i < 4; i++) {
    int64_t v = i * 100;
    kl_binary_sparseset_set(&fx.set, 10 + i, &v);
  }

  // Remove id 11 (dense pos 1). The last element (id 13, dense pos 3) should
  // be swapped in, so:
  //   dense[1] == bytes of (id 13, value 300)
  //   sparse[13] == 1
  //   indices[1] == 13
  KL_TEST_ASSERT(kl_binary_sparseset_remove(&fx.set, 11), "remove returns true");
  KL_TEST_EXPECT_EQ_INT((int)fx.set.length, 3);
  KL_TEST_ASSERT(!kl_binary_sparseset_has(&fx.set, 11), "11 gone");
  KL_TEST_ASSERT(kl_binary_sparseset_has(&fx.set, 13), "13 still present");

  int64_t* got = (int64_t*)kl_binary_sparseset_get(&fx.set, 13);
  KL_TEST_ASSERT(got != 0 && *got == 300, "13 value preserved through swap");
  KL_TEST_EXPECT_EQ_INT((int)fx.set.sparse[13], 1);
  KL_TEST_EXPECT_EQ_INT((int)fx.set.indices[1], 13);

  // Untouched ids still map correctly.
  KL_TEST_EXPECT_EQ_INT((int)fx.set.indices[0], 10);
  KL_TEST_EXPECT_EQ_INT((int)fx.set.indices[2], 12);
  KL_TEST_ASSERT(*(int64_t*)kl_binary_sparseset_get(&fx.set, 10) == 0, "10 value");
  KL_TEST_ASSERT(*(int64_t*)kl_binary_sparseset_get(&fx.set, 12) == 200, "12 value");

  kl_test_bss_teardown(&fx);
}

void test_binary_sparseset_remove_then_reinsert_same_id() {
  KL_TestBSS fx = {0};
  kl_test_bss_setup(&fx, sizeof(int64_t), _Alignof(int64_t), 16, 8);

  int64_t v = 42;
  kl_binary_sparseset_set(&fx.set, 7, &v);
  KL_TEST_ASSERT(kl_binary_sparseset_remove(&fx.set, 7), "first remove");
  KL_TEST_ASSERT(!kl_binary_sparseset_has(&fx.set, 7), "gone after remove");
  KL_TEST_EXPECT_EQ_INT((int)fx.set.length, 0);

  v = 4242;
  kl_binary_sparseset_set(&fx.set, 7, &v);
  KL_TEST_ASSERT(kl_binary_sparseset_has(&fx.set, 7), "reinserted");
  KL_TEST_ASSERT(*(int64_t*)kl_binary_sparseset_get(&fx.set, 7) == 4242, "new value visible");
  KL_TEST_EXPECT_EQ_INT((int)fx.set.length, 1);

  kl_test_bss_teardown(&fx);
}

void test_binary_sparseset_remove_all_drains() {
  KL_TestBSS fx = {0};
  kl_test_bss_setup(&fx, sizeof(int64_t), _Alignof(int64_t), 16, 8);

  for (int64_t i = 0; i < 8; i++) {
    int64_t v = i;
    kl_binary_sparseset_set(&fx.set, i, &v);
  }
  KL_TEST_EXPECT_EQ_INT((int)fx.set.length, 8);

  // Repeatedly remove the front of the dense buffer; each step swaps the
  // last element into position 0.
  for (int64_t i = 0; i < 8; i++) {
    int64_t id_at_front = fx.set.indices[0];
    KL_TEST_ASSERT(kl_binary_sparseset_remove(&fx.set, id_at_front), "remove front");
    KL_TEST_EXPECT_EQ_INT((int)fx.set.length, (int)(7 - i));
    KL_TEST_ASSERT(!kl_binary_sparseset_has(&fx.set, id_at_front), "front id gone");
  }
  KL_TEST_EXPECT_EQ_INT((int)fx.set.length, 0);
  for (int64_t i = 0; i < 8; i++) {
    KL_TEST_ASSERT(!kl_binary_sparseset_has(&fx.set, i), "all ids gone");
  }

  kl_test_bss_teardown(&fx);
}

void test_binary_sparseset_reset_clears_entries() {
  KL_TestBSS fx = {0};
  kl_test_bss_setup(&fx, sizeof(int64_t), _Alignof(int64_t), 16, 8);

  for (int64_t i = 0; i < 5; i++) {
    int64_t v = 1000 + i;
    kl_binary_sparseset_set(&fx.set, i, &v);
  }
  KL_TEST_EXPECT_EQ_INT((int)fx.set.length, 5);

  kl_binary_sparseset_reset(&fx.set);
  KL_TEST_EXPECT_EQ_INT((int)fx.set.length, 0);
  for (int64_t i = 0; i < fx.set.sparse_capacity; i++) {
    KL_TEST_ASSERT(fx.set.sparse[i] == -1, "sparse slot reset to -1");
    KL_TEST_ASSERT(!kl_binary_sparseset_has(&fx.set, i), "has=false after reset");
    KL_TEST_ASSERT(kl_binary_sparseset_get(&fx.set, i) == 0, "get=null after reset");
  }

  kl_test_bss_teardown(&fx);
}

void test_binary_sparseset_reset_preserves_capacity() {
  KL_TestBSS fx = {0};
  kl_test_bss_setup(&fx, sizeof(int64_t), _Alignof(int64_t), 16, 8);

  // Force a sparse grow.
  int64_t v = 1;
  kl_binary_sparseset_set(&fx.set, 64, &v);
  int64_t sparse_cap = fx.set.sparse_capacity;
  int64_t dense_cap = fx.set.dense_capacity;
  KL_TEST_ASSERT(sparse_cap > 16, "sparse grew past initial capacity");

  kl_binary_sparseset_reset(&fx.set);
  KL_TEST_EXPECT_EQ_INT((int)fx.set.sparse_capacity, (int)sparse_cap);
  KL_TEST_EXPECT_EQ_INT((int)fx.set.dense_capacity, (int)dense_cap);
  KL_TEST_ASSERT(fx.set.initialized == true, "still initialized after reset");

  // Reset state is reusable.
  kl_binary_sparseset_set(&fx.set, 3, &v);
  KL_TEST_ASSERT(kl_binary_sparseset_has(&fx.set, 3), "usable after reset");

  kl_test_bss_teardown(&fx);
}

void test_binary_sparseset_reset_on_empty_is_safe() {
  KL_TestBSS fx = {0};
  kl_test_bss_setup(&fx, sizeof(int64_t), _Alignof(int64_t), 16, 8);

  kl_binary_sparseset_reset(&fx.set);
  KL_TEST_EXPECT_EQ_INT((int)fx.set.length, 0);

  // Reset on a non-initialized set is also a no-op.
  KL_BinarySparseSet empty = {0};
  kl_binary_sparseset_reset(&empty);
  KL_TEST_ASSERT(empty.initialized == false, "uninit-reset stays uninit");

  kl_test_bss_teardown(&fx);
}

void test_binary_sparseset_sparse_grows_for_large_id() {
  KL_TestBSS fx = {0};
  kl_test_bss_setup(&fx, sizeof(int64_t), _Alignof(int64_t), 8, 8);

  int64_t initial_cap = fx.set.sparse_capacity;
  KL_TEST_EXPECT_EQ_INT((int)initial_cap, 8);

  // Insert an id that forces the sparse array to double several times.
  int64_t v = 777;
  kl_binary_sparseset_set(&fx.set, 100, &v);
  KL_TEST_ASSERT(fx.set.sparse_capacity > 100, "sparse capacity now covers id 100");
  // Doubling sequence starting from 8: 16, 32, 64, 128. First power >= 101 is 128.
  KL_TEST_EXPECT_EQ_INT((int)fx.set.sparse_capacity, 128);

  // All newly-grown slots must be -1, not garbage.
  for (int64_t i = initial_cap; i < fx.set.sparse_capacity; i++) {
    if (i == 100) continue;
    KL_TEST_ASSERT(fx.set.sparse[i] == -1, "new sparse slots default to -1");
  }
  KL_TEST_ASSERT(*(int64_t*)kl_binary_sparseset_get(&fx.set, 100) == 777, "stored value");
  KL_TEST_ASSERT(!kl_binary_sparseset_has(&fx.set, 99), "99 still absent");
  KL_TEST_ASSERT(!kl_binary_sparseset_has(&fx.set, 127), "127 still absent");

  kl_test_bss_teardown(&fx);
}

void test_binary_sparseset_dense_grows_when_full() {
  KL_TestBSS fx = {0};
  kl_test_bss_setup(&fx, sizeof(int64_t), _Alignof(int64_t), 64, 4);

  int64_t initial_dense_cap = fx.set.dense_capacity;
  KL_TEST_EXPECT_EQ_INT((int)initial_dense_cap, 4);

  // Fill past the initial dense capacity to force multiple doublings.
  for (int64_t i = 0; i < 20; i++) {
    int64_t v = i * i;
    kl_binary_sparseset_set(&fx.set, i, &v);
  }
  KL_TEST_EXPECT_EQ_INT((int)fx.set.length, 20);
  KL_TEST_ASSERT(fx.set.dense_capacity >= 20, "dense capacity grew to fit");

  for (int64_t i = 0; i < 20; i++) {
    int64_t* got = (int64_t*)kl_binary_sparseset_get(&fx.set, i);
    KL_TEST_ASSERT(got != 0, "every id retrievable");
    KL_TEST_ASSERT(*got == i * i, "values preserved across dense grow");
  }

  kl_test_bss_teardown(&fx);
}

void test_binary_sparseset_at_iterates_dense_buffer() {
  KL_TestBSS fx = {0};
  kl_test_bss_setup(&fx, sizeof(int64_t), _Alignof(int64_t), 16, 8);

  // Insertion order: ids 5, 2, 9, 1 -> dense positions 0..3.
  int64_t order[] = { 5, 2, 9, 1 };
  for (size_t i = 0; i < KL_ARRAY_LEN(order); i++) {
    int64_t v = order[i] * 10;
    kl_binary_sparseset_set(&fx.set, order[i], &v);
  }
  KL_TEST_EXPECT_EQ_INT((int)fx.set.length, (int)KL_ARRAY_LEN(order));

  for (int64_t i = 0; i < fx.set.length; i++) {
    int64_t* p = (int64_t*)kl_binary_sparseset_at(&fx.set, i);
    KL_TEST_ASSERT(p != 0, "at returns non-null");
    KL_TEST_EXPECT_EQ_INT((int)*p, (int)(order[i] * 10));
    KL_TEST_EXPECT_EQ_INT((int)fx.set.indices[i], (int)order[i]);
  }

  kl_test_bss_teardown(&fx);
}

void test_binary_sparseset_struct_payload_round_trip() {
  typedef struct { int32_t x; int32_t y; int64_t tag; } Point;

  KL_TestBSS fx = {0};
  kl_test_bss_setup(&fx, (int64_t)sizeof(Point), (int64_t)_Alignof(Point), 32, 8);

  enum { N = 12 };
  Point inputs[N];
  int64_t ids[N];
  for (int i = 0; i < N; i++) {
    inputs[i] = (Point){ .x = i * 3, .y = -i, .tag = (int64_t)i * 1000 + 7 };
    ids[i] = i * 2 + 1;
    kl_binary_sparseset_set(&fx.set, ids[i], &inputs[i]);
  }
  KL_TEST_EXPECT_EQ_INT((int)fx.set.length, N);
  KL_TEST_ASSERT(((uintptr_t)fx.set.values % (uintptr_t)_Alignof(Point)) == 0, "values aligned for Point");

  for (int i = 0; i < N; i++) {
    Point* got = (Point*)kl_binary_sparseset_get(&fx.set, ids[i]);
    KL_TEST_ASSERT(got != 0, "struct get non-null");
    KL_TEST_EXPECT_EQ_INT(got->x, inputs[i].x);
    KL_TEST_EXPECT_EQ_INT(got->y, inputs[i].y);
    KL_TEST_ASSERT(got->tag == inputs[i].tag, "tag field preserved");
  }

  // Mutate in place through the returned pointer and re-read.
  Point* p = (Point*)kl_binary_sparseset_get(&fx.set, ids[3]);
  p->x = 9999;
  Point* p2 = (Point*)kl_binary_sparseset_get(&fx.set, ids[3]);
  KL_TEST_EXPECT_EQ_INT(p2->x, 9999);

  kl_test_bss_teardown(&fx);
}

void test_binary_sparseset_many_inserts_round_trip() {
  KL_TestBSS fx = {0};
  kl_test_bss_setup(&fx, sizeof(int64_t), _Alignof(int64_t), 4, 4);

  enum { N = 1000 };
  for (int64_t i = 0; i < N; i++) {
    int64_t v = i * 7 - 3;
    kl_binary_sparseset_set(&fx.set, i, &v);
  }
  KL_TEST_EXPECT_EQ_INT((int)fx.set.length, N);
  KL_TEST_ASSERT(fx.set.sparse_capacity >= N, "sparse covers all ids");
  KL_TEST_ASSERT(fx.set.dense_capacity >= N, "dense covers all ids");

  for (int64_t i = 0; i < N; i++) {
    int64_t* got = (int64_t*)kl_binary_sparseset_get(&fx.set, i);
    KL_TEST_ASSERT(got != 0, "id present after mass insert");
    KL_TEST_ASSERT(*got == i * 7 - 3, "value preserved");
  }

  // Remove every other id, then verify the survivors are still correct.
  for (int64_t i = 0; i < N; i += 2) {
    KL_TEST_ASSERT(kl_binary_sparseset_remove(&fx.set, i), "remove even id");
  }
  KL_TEST_EXPECT_EQ_INT((int)fx.set.length, N / 2);
  for (int64_t i = 0; i < N; i++) {
    if ((i & 1) == 0) {
      KL_TEST_ASSERT(!kl_binary_sparseset_has(&fx.set, i), "even gone");
    } else {
      int64_t* got = (int64_t*)kl_binary_sparseset_get(&fx.set, i);
      KL_TEST_ASSERT(got != 0 && *got == i * 7 - 3, "odd survives with right value");
    }
  }

  kl_test_bss_teardown(&fx);
}

void test_binary_sparseset_alignment_default() {
  // element_alignment = 0 should still produce a usable set (treated as 1).
  KL_TestBSS fx = {0};
  kl_test_bss_setup(&fx, sizeof(int8_t), 0, 8, 8);
  KL_TEST_EXPECT_EQ_INT((int)fx.set.alignment, 1);

  for (int8_t i = 0; i < 8; i++) {
    int8_t v = (int8_t)(0x10 + i);
    kl_binary_sparseset_set(&fx.set, i, &v);
  }
  for (int8_t i = 0; i < 8; i++) {
    int8_t* got = (int8_t*)kl_binary_sparseset_get(&fx.set, i);
    KL_TEST_ASSERT(got != 0 && *got == (int8_t)(0x10 + i), "byte stride round-trip");
  }

  kl_test_bss_teardown(&fx);
}

void test_binary_sparseset_id_zero_works() {
  // id 0 is a valid entity id; the -1 sentinel must not be confused with it.
  KL_TestBSS fx = {0};
  kl_test_bss_setup(&fx, sizeof(int64_t), _Alignof(int64_t), 16, 8);

  KL_TEST_ASSERT(!kl_binary_sparseset_has(&fx.set, 0), "0 not present initially");
  int64_t v = 0xDEAD;
  kl_binary_sparseset_set(&fx.set, 0, &v);
  KL_TEST_ASSERT(kl_binary_sparseset_has(&fx.set, 0), "0 present after set");
  KL_TEST_ASSERT(*(int64_t*)kl_binary_sparseset_get(&fx.set, 0) == 0xDEAD, "0 stores value");
  KL_TEST_ASSERT(kl_binary_sparseset_remove(&fx.set, 0), "0 removable");
  KL_TEST_ASSERT(!kl_binary_sparseset_has(&fx.set, 0), "0 gone after remove");

  kl_test_bss_teardown(&fx);
}

// ---------- no-allocator variants ----------
// Same surface as the arena-backed tests above, but with cfg.allocator == NULL
// so the set falls back to kl_aligned_alloc / kl_aligned_realloc / free.

static void kl_test_bss_setup_noalloc(KL_BinarySparseSet* set, int64_t element_size,
                                      int64_t element_alignment,
                                      int64_t sparse_capacity, int64_t dense_capacity) {
  *set = (KL_BinarySparseSet){0};
  kl_binary_sparseset_init(set, (KL_BinarySparseSetConfig){
    .sparse_capacity = sparse_capacity,
    .dense_capacity = dense_capacity,
    .element_size = element_size,
    .element_alignment = element_alignment,
    .allocator = 0,
  });
}

void test_binary_sparseset_noalloc_init_destroy() {
  KL_BinarySparseSet set = {0};
  kl_test_bss_setup_noalloc(&set, sizeof(int64_t), _Alignof(int64_t), 16, 8);

  KL_TEST_ASSERT(set.initialized == true, "initialized after init");
  KL_TEST_ASSERT(set.config.allocator == 0, "allocator stays null");
  KL_TEST_EXPECT_EQ_INT((int)set.sparse_capacity, 16);
  KL_TEST_EXPECT_EQ_INT((int)set.dense_capacity, 8);
  KL_TEST_ASSERT(set.sparse != 0, "sparse buffer allocated via kl_aligned_alloc");
  KL_TEST_ASSERT(set.indices != 0, "indices buffer allocated via kl_aligned_alloc");
  KL_TEST_ASSERT(set.values != 0, "values buffer allocated via kl_aligned_alloc");
  for (int64_t i = 0; i < set.sparse_capacity; i++) {
    KL_TEST_ASSERT(set.sparse[i] == -1, "sparse slot defaults to -1");
  }

  kl_binary_sparseset_destroy(&set);
  KL_TEST_ASSERT(set.initialized == false, "initialized cleared after destroy");
  KL_TEST_ASSERT(set.sparse == 0, "sparse pointer cleared");
  KL_TEST_ASSERT(set.indices == 0, "indices pointer cleared");
  KL_TEST_ASSERT(set.values == 0, "values pointer cleared");
}

void test_binary_sparseset_noalloc_set_get_basic() {
  KL_BinarySparseSet set = {0};
  kl_test_bss_setup_noalloc(&set, sizeof(int64_t), _Alignof(int64_t), 16, 8);

  int64_t v = 12345;
  void* p = kl_binary_sparseset_set(&set, 3, &v);
  KL_TEST_ASSERT(p != 0, "set returns non-null pointer");
  KL_TEST_EXPECT_EQ_INT((int)set.length, 1);

  int64_t* got = (int64_t*)kl_binary_sparseset_get(&set, 3);
  KL_TEST_ASSERT(got != 0, "get returns non-null for inserted id");
  KL_TEST_ASSERT(*got == 12345, "value round-trips");

  kl_binary_sparseset_destroy(&set);
}

void test_binary_sparseset_noalloc_sparse_grows_for_large_id() {
  KL_BinarySparseSet set = {0};
  kl_test_bss_setup_noalloc(&set, sizeof(int64_t), _Alignof(int64_t), 8, 8);

  int64_t initial_cap = set.sparse_capacity;
  KL_TEST_EXPECT_EQ_INT((int)initial_cap, 8);

  // Insert an id large enough to force the sparse array to double several times.
  // This exercises the kl_aligned_realloc fallback path.
  int64_t v = 777;
  kl_binary_sparseset_set(&set, 100, &v);
  KL_TEST_ASSERT(set.sparse_capacity > 100, "sparse capacity now covers id 100");
  KL_TEST_EXPECT_EQ_INT((int)set.sparse_capacity, 128);

  for (int64_t i = initial_cap; i < set.sparse_capacity; i++) {
    if (i == 100) continue;
    KL_TEST_ASSERT(set.sparse[i] == -1, "new sparse slots default to -1");
  }
  KL_TEST_ASSERT(*(int64_t*)kl_binary_sparseset_get(&set, 100) == 777, "stored value");

  kl_binary_sparseset_destroy(&set);
}

void test_binary_sparseset_noalloc_dense_grows_when_full() {
  KL_BinarySparseSet set = {0};
  kl_test_bss_setup_noalloc(&set, sizeof(int64_t), _Alignof(int64_t), 64, 4);

  KL_TEST_EXPECT_EQ_INT((int)set.dense_capacity, 4);

  // Force multiple dense doublings — both indices and values go through
  // kl_aligned_realloc.
  for (int64_t i = 0; i < 20; i++) {
    int64_t v = i * i;
    kl_binary_sparseset_set(&set, i, &v);
  }
  KL_TEST_EXPECT_EQ_INT((int)set.length, 20);
  KL_TEST_ASSERT(set.dense_capacity >= 20, "dense capacity grew to fit");

  for (int64_t i = 0; i < 20; i++) {
    int64_t* got = (int64_t*)kl_binary_sparseset_get(&set, i);
    KL_TEST_ASSERT(got != 0, "every id retrievable");
    KL_TEST_ASSERT(*got == i * i, "values preserved across dense grow");
  }

  kl_binary_sparseset_destroy(&set);
}

void test_binary_sparseset_noalloc_many_inserts_round_trip() {
  KL_BinarySparseSet set = {0};
  kl_test_bss_setup_noalloc(&set, sizeof(int64_t), _Alignof(int64_t), 4, 4);

  enum { N = 1000 };
  for (int64_t i = 0; i < N; i++) {
    int64_t v = i * 7 - 3;
    kl_binary_sparseset_set(&set, i, &v);
  }
  KL_TEST_EXPECT_EQ_INT((int)set.length, N);
  KL_TEST_ASSERT(set.sparse_capacity >= N, "sparse covers all ids");
  KL_TEST_ASSERT(set.dense_capacity >= N, "dense covers all ids");

  for (int64_t i = 0; i < N; i++) {
    int64_t* got = (int64_t*)kl_binary_sparseset_get(&set, i);
    KL_TEST_ASSERT(got != 0, "id present after mass insert");
    KL_TEST_ASSERT(*got == i * 7 - 3, "value preserved");
  }

  // Remove every other id — exercises swap-and-pop without an arena.
  for (int64_t i = 0; i < N; i += 2) {
    KL_TEST_ASSERT(kl_binary_sparseset_remove(&set, i), "remove even id");
  }
  KL_TEST_EXPECT_EQ_INT((int)set.length, N / 2);
  for (int64_t i = 1; i < N; i += 2) {
    int64_t* got = (int64_t*)kl_binary_sparseset_get(&set, i);
    KL_TEST_ASSERT(got != 0 && *got == i * 7 - 3, "odd survives with right value");
  }

  kl_binary_sparseset_destroy(&set);
}

void test_binary_sparseset_noalloc_reset_then_reuse() {
  KL_BinarySparseSet set = {0};
  kl_test_bss_setup_noalloc(&set, sizeof(int64_t), _Alignof(int64_t), 16, 8);

  // Trigger a sparse grow so reset must preserve the grown capacity.
  int64_t v = 1;
  kl_binary_sparseset_set(&set, 64, &v);
  int64_t sparse_cap = set.sparse_capacity;
  KL_TEST_ASSERT(sparse_cap > 16, "sparse grew");

  kl_binary_sparseset_reset(&set);
  KL_TEST_EXPECT_EQ_INT((int)set.length, 0);
  KL_TEST_EXPECT_EQ_INT((int)set.sparse_capacity, (int)sparse_cap);

  kl_binary_sparseset_set(&set, 3, &v);
  KL_TEST_ASSERT(kl_binary_sparseset_has(&set, 3), "usable after reset");

  kl_binary_sparseset_destroy(&set);
}

// ---------- templated sparse_set (dummy), no allocator ----------
// The dummy template is compiled into libkl.a as a concrete instantiation:
// NAME is left literal and the item type is T_X (== int). These tests drive
// the allocator-optional path (cfg.allocator == 0 falls back to
// kl_aligned_alloc / kl_aligned_realloc / free).

static void kl_test_ss_setup_noalloc(KL_NAME_SparseSet* set,
                                     int64_t sparse_capacity, int64_t dense_capacity) {
  *set = (KL_NAME_SparseSet){0};
  kl_NAME_sparse_set_init(set, (KL_SparseSetConfig){
    .sparse_capacity = sparse_capacity,
    .dense_capacity = dense_capacity,
    .allocator = 0,
  });
}

void test_sparse_set_noalloc_init_destroy() {
  KL_NAME_SparseSet set = {0};
  kl_test_ss_setup_noalloc(&set, 16, 8);

  KL_TEST_ASSERT(set.initialized == true, "initialized after init");
  KL_TEST_ASSERT(set.config.allocator == 0, "allocator stays null");
  KL_TEST_EXPECT_EQ_INT((int)set.length, 0);
  KL_TEST_EXPECT_EQ_INT((int)set.sparse_capacity, 16);
  KL_TEST_EXPECT_EQ_INT((int)set.dense_capacity, 8);
  KL_TEST_ASSERT(set.sparse != 0, "sparse buffer allocated via kl_aligned_alloc");
  KL_TEST_ASSERT(set.indices != 0, "indices buffer allocated via kl_aligned_alloc");
  KL_TEST_ASSERT(set.values != 0, "values buffer allocated via kl_aligned_alloc");
  for (int64_t i = 0; i < set.sparse_capacity; i++) {
    KL_TEST_ASSERT(set.sparse[i] == -1, "sparse slot defaults to -1");
  }

  kl_NAME_sparse_set_destroy(&set);
  KL_TEST_ASSERT(set.initialized == false, "initialized cleared after destroy");
  KL_TEST_ASSERT(set.sparse == 0, "sparse pointer cleared");
  KL_TEST_ASSERT(set.indices == 0, "indices pointer cleared");
  KL_TEST_ASSERT(set.values == 0, "values pointer cleared");
  KL_TEST_EXPECT_EQ_INT((int)set.length, 0);
  KL_TEST_EXPECT_EQ_INT((int)set.sparse_capacity, 0);
  KL_TEST_EXPECT_EQ_INT((int)set.dense_capacity, 0);
}

void test_sparse_set_noalloc_set_get_basic() {
  KL_NAME_SparseSet set = {0};
  kl_test_ss_setup_noalloc(&set, 16, 8);

  int* p = kl_NAME_sparse_set_set(&set, 3, 12345);
  KL_TEST_ASSERT(p != 0, "set returns non-null pointer");
  KL_TEST_ASSERT(*p == 12345, "set returns pointer to the stored value");
  KL_TEST_EXPECT_EQ_INT((int)set.length, 1);

  int* got = kl_NAME_sparse_set_get(&set, 3);
  KL_TEST_ASSERT(got != 0, "get returns non-null for inserted id");
  KL_TEST_ASSERT(got == p, "get returns the same pointer as set");
  KL_TEST_ASSERT(*got == 12345, "value round-trips");

  KL_TEST_ASSERT(kl_NAME_sparse_set_get(&set, 0) == 0, "get on unset id returns null");
  KL_TEST_ASSERT(kl_NAME_sparse_set_get(&set, 4) == 0, "neighbouring id still null");

  kl_NAME_sparse_set_destroy(&set);
}

void test_sparse_set_noalloc_set_overwrite_keeps_length() {
  KL_NAME_SparseSet set = {0};
  kl_test_ss_setup_noalloc(&set, 16, 8);

  kl_NAME_sparse_set_set(&set, 5, 1);
  KL_TEST_EXPECT_EQ_INT((int)set.length, 1);

  for (int i = 0; i < 10; i++) {
    int* p = kl_NAME_sparse_set_set(&set, 5, 100 + i);
    KL_TEST_ASSERT(p != 0 && *p == 100 + i, "overwrite returns the updated value");
    KL_TEST_EXPECT_EQ_INT((int)set.length, 1);
  }
  KL_TEST_ASSERT(*kl_NAME_sparse_set_get(&set, 5) == 109, "final overwrite visible");

  kl_NAME_sparse_set_destroy(&set);
}

void test_sparse_set_noalloc_has_and_remove() {
  KL_NAME_SparseSet set = {0};
  kl_test_ss_setup_noalloc(&set, 16, 8);

  KL_TEST_ASSERT(!kl_NAME_sparse_set_has(&set, 4), "has=false before set");
  KL_TEST_ASSERT(!kl_NAME_sparse_set_remove(&set, 4), "remove missing => false");

  kl_NAME_sparse_set_set(&set, 4, 7);
  KL_TEST_ASSERT(kl_NAME_sparse_set_has(&set, 4), "has=true after set");
  KL_TEST_ASSERT(!kl_NAME_sparse_set_has(&set, 3), "neighbour absent");

  KL_TEST_ASSERT(kl_NAME_sparse_set_remove(&set, 4), "remove existing => true");
  KL_TEST_ASSERT(!kl_NAME_sparse_set_has(&set, 4), "gone after remove");
  KL_TEST_ASSERT(kl_NAME_sparse_set_get(&set, 4) == 0, "get null after remove");
  KL_TEST_EXPECT_EQ_INT((int)set.length, 0);

  // Out-of-range ids are rejected, not treated as present.
  KL_TEST_ASSERT(!kl_NAME_sparse_set_has(&set, -1), "negative id => false");
  KL_TEST_ASSERT(kl_NAME_sparse_set_get(&set, -1) == 0, "negative id get => null");
  KL_TEST_ASSERT(kl_NAME_sparse_set_get(&set, set.sparse_capacity) == 0, "id == capacity get => null");

  kl_NAME_sparse_set_destroy(&set);
}

void test_sparse_set_noalloc_id_zero_and_value_zero() {
  KL_NAME_SparseSet set = {0};
  kl_test_ss_setup_noalloc(&set, 16, 8);

  // id 0 is valid and value 0 is a real stored value — neither must be
  // confused with the -1 "absent" sentinel kept in the sparse array.
  KL_TEST_ASSERT(!kl_NAME_sparse_set_has(&set, 0), "id 0 absent initially");
  int* p = kl_NAME_sparse_set_set(&set, 0, 0);
  KL_TEST_ASSERT(p != 0, "set id 0 with value 0 returns non-null");
  KL_TEST_ASSERT(kl_NAME_sparse_set_has(&set, 0), "id 0 present after set");
  int* got = kl_NAME_sparse_set_get(&set, 0);
  KL_TEST_ASSERT(got != 0 && *got == 0, "value 0 round-trips as a present entry");
  KL_TEST_ASSERT(kl_NAME_sparse_set_remove(&set, 0), "id 0 removable");
  KL_TEST_ASSERT(!kl_NAME_sparse_set_has(&set, 0), "id 0 gone after remove");

  kl_NAME_sparse_set_destroy(&set);
}

void test_sparse_set_noalloc_sparse_grows_for_large_id() {
  KL_NAME_SparseSet set = {0};
  kl_test_ss_setup_noalloc(&set, 8, 8);

  int64_t initial_cap = set.sparse_capacity;
  KL_TEST_EXPECT_EQ_INT((int)initial_cap, 8);

  // Forces the sparse array through kl_aligned_realloc: 8 -> 16 -> 32 -> 64 -> 128.
  kl_NAME_sparse_set_set(&set, 100, 777);
  KL_TEST_ASSERT(set.sparse_capacity > 100, "sparse capacity now covers id 100");
  KL_TEST_EXPECT_EQ_INT((int)set.sparse_capacity, 128);

  for (int64_t i = initial_cap; i < set.sparse_capacity; i++) {
    if (i == 100) continue;
    KL_TEST_ASSERT(set.sparse[i] == -1, "newly grown sparse slots default to -1");
  }
  KL_TEST_ASSERT(*kl_NAME_sparse_set_get(&set, 100) == 777, "stored value survives the grow");
  KL_TEST_ASSERT(!kl_NAME_sparse_set_has(&set, 99), "99 still absent");

  kl_NAME_sparse_set_destroy(&set);
}

void test_sparse_set_noalloc_dense_grows_when_full() {
  KL_NAME_SparseSet set = {0};
  kl_test_ss_setup_noalloc(&set, 64, 4);

  KL_TEST_EXPECT_EQ_INT((int)set.dense_capacity, 4);

  // Multiple dense doublings push both indices and values through kl_aligned_realloc.
  for (int i = 0; i < 20; i++) {
    kl_NAME_sparse_set_set(&set, i, i * i);
  }
  KL_TEST_EXPECT_EQ_INT((int)set.length, 20);
  KL_TEST_ASSERT(set.dense_capacity >= 20, "dense capacity grew to fit");

  for (int i = 0; i < 20; i++) {
    int* got = kl_NAME_sparse_set_get(&set, i);
    KL_TEST_ASSERT(got != 0 && *got == i * i, "values preserved across dense grow");
  }

  kl_NAME_sparse_set_destroy(&set);
}

void test_sparse_set_noalloc_remove_swaps_with_last() {
  KL_NAME_SparseSet set = {0};
  kl_test_ss_setup_noalloc(&set, 16, 8);

  // Insert ids 10,11,12,13 -> dense positions 0,1,2,3.
  for (int i = 0; i < 4; i++) {
    kl_NAME_sparse_set_set(&set, 10 + i, i * 100);
  }

  // Removing id 11 (dense pos 1) swaps the last element (id 13) into pos 1.
  KL_TEST_ASSERT(kl_NAME_sparse_set_remove(&set, 11), "remove returns true");
  KL_TEST_EXPECT_EQ_INT((int)set.length, 3);
  KL_TEST_ASSERT(!kl_NAME_sparse_set_has(&set, 11), "11 gone");
  KL_TEST_ASSERT(kl_NAME_sparse_set_has(&set, 13), "13 still present");
  KL_TEST_ASSERT(*kl_NAME_sparse_set_get(&set, 13) == 300, "13 value preserved through swap");
  KL_TEST_EXPECT_EQ_INT((int)set.sparse[13], 1);
  KL_TEST_EXPECT_EQ_INT((int)set.indices[1], 13);

  // Untouched ids still map correctly.
  KL_TEST_ASSERT(*kl_NAME_sparse_set_get(&set, 10) == 0, "10 value intact");
  KL_TEST_ASSERT(*kl_NAME_sparse_set_get(&set, 12) == 200, "12 value intact");

  kl_NAME_sparse_set_destroy(&set);
}

void test_sparse_set_noalloc_many_inserts_round_trip() {
  KL_NAME_SparseSet set = {0};
  kl_test_ss_setup_noalloc(&set, 4, 4);

  enum { N = 1000 };
  for (int i = 0; i < N; i++) {
    kl_NAME_sparse_set_set(&set, i, i * 7 - 3);
  }
  KL_TEST_EXPECT_EQ_INT((int)set.length, N);
  KL_TEST_ASSERT(set.sparse_capacity >= N, "sparse covers all ids");
  KL_TEST_ASSERT(set.dense_capacity >= N, "dense covers all ids");

  for (int i = 0; i < N; i++) {
    int* got = kl_NAME_sparse_set_get(&set, i);
    KL_TEST_ASSERT(got != 0 && *got == i * 7 - 3, "value preserved after mass insert");
  }

  // Remove every other id — swap-and-pop without an arena.
  for (int i = 0; i < N; i += 2) {
    KL_TEST_ASSERT(kl_NAME_sparse_set_remove(&set, i), "remove even id");
  }
  KL_TEST_EXPECT_EQ_INT((int)set.length, N / 2);
  for (int i = 0; i < N; i++) {
    if ((i & 1) == 0) {
      KL_TEST_ASSERT(!kl_NAME_sparse_set_has(&set, i), "even gone");
    } else {
      int* got = kl_NAME_sparse_set_get(&set, i);
      KL_TEST_ASSERT(got != 0 && *got == i * 7 - 3, "odd survives with right value");
    }
  }

  kl_NAME_sparse_set_destroy(&set);
}

void test_sparse_set_noalloc_reset_then_reuse() {
  KL_NAME_SparseSet set = {0};
  kl_test_ss_setup_noalloc(&set, 16, 8);

  // Trigger a sparse grow so reset must preserve the grown capacity.
  kl_NAME_sparse_set_set(&set, 64, 1);
  int64_t sparse_cap = set.sparse_capacity;
  KL_TEST_ASSERT(sparse_cap > 16, "sparse grew");

  kl_NAME_sparse_set_reset(&set);
  KL_TEST_EXPECT_EQ_INT((int)set.length, 0);
  KL_TEST_EXPECT_EQ_INT((int)set.sparse_capacity, (int)sparse_cap);
  KL_TEST_ASSERT(set.initialized == true, "still initialized after reset");
  KL_TEST_ASSERT(!kl_NAME_sparse_set_has(&set, 64), "entries cleared after reset");

  // Reusable after reset.
  kl_NAME_sparse_set_set(&set, 3, 42);
  KL_TEST_ASSERT(kl_NAME_sparse_set_has(&set, 3), "usable after reset");
  KL_TEST_ASSERT(*kl_NAME_sparse_set_get(&set, 3) == 42, "value visible after reuse");

  kl_NAME_sparse_set_destroy(&set);
}
