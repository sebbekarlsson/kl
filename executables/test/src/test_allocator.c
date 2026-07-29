#include <test/tests.h>

void test_allocator_init_destroy() {
  KL_Allocator a = {0};
  KL_AllocatorConfig cfg = {
    .capacity = 256,
    .tracked_allocations_capacity = 16,
    .track_allocations = false,
  };
  KL_TEST_EXPECT_EQ_INT(kl_allocator_init(&a, cfg), 1);
  KL_TEST_ASSERT(a.initialized == true, "initialized flag set after init");
  KL_TEST_EXPECT_EQ_INT(a.pages.length, 0);
  KL_TEST_ASSERT(a.pages.items == 0, "no pages buffer before first alloc");
  KL_TEST_EXPECT_EQ_INT(a.allocations.length, 0);
  KL_TEST_EXPECT_EQ_INT(a.allocations.n_allocated, 0);
  KL_TEST_ASSERT(a.current_capacity >= 256, "capacity at least requested");

  kl_allocator_destroy(&a);
  KL_TEST_ASSERT(a.initialized == false, "initialized flag cleared after destroy");
  KL_TEST_ASSERT(a.pages.items == 0, "pages buffer freed after destroy");
  KL_TEST_EXPECT_EQ_INT(a.pages.length, 0);
}

void test_allocator_double_init_is_noop() {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 128 });
  int64_t cap_after_first = a.current_capacity;
  // Calling init again on an initialized allocator must be a no-op.
  KL_TEST_EXPECT_EQ_INT(kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 9999 }), 1);
  KL_TEST_EXPECT_EQ_INT(a.current_capacity, cap_after_first);
  kl_allocator_destroy(&a);
}

void test_allocator_basic_malloc() {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 1024 });

  uint8_t* p = kl_allocator_malloc(&a, 64);
  KL_TEST_ASSERT(p != 0, "malloc returns non-null");
  for (int i = 0; i < 64; i++) p[i] = (uint8_t)(i & 0xFF);
  for (int i = 0; i < 64; i++) KL_TEST_EXPECT_EQ_INT(p[i], (uint8_t)(i & 0xFF));
  KL_TEST_EXPECT_EQ_INT(a.pages.length, 1);

  kl_allocator_destroy(&a);
}

void test_allocator_zero_initialized_memory() {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 256 });
  uint8_t* p = kl_allocator_malloc(&a, 128);
  for (int i = 0; i < 128; i++) KL_TEST_EXPECT_EQ_INT(p[i], 0);
  kl_allocator_destroy(&a);
}

void test_allocator_multiple_mallocs_no_overlap() {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 1024 });

  uint8_t* p1 = kl_allocator_malloc(&a, 64);
  uint8_t* p2 = kl_allocator_malloc(&a, 64);
  uint8_t* p3 = kl_allocator_malloc(&a, 64);

  KL_TEST_ASSERT(p1 != 0 && p2 != 0 && p3 != 0, "all allocs succeed");
  KL_TEST_ASSERT(p1 != p2 && p2 != p3 && p1 != p3, "distinct pointers");

  memset(p1, 0xAA, 64);
  memset(p2, 0xBB, 64);
  memset(p3, 0xCC, 64);
  for (int i = 0; i < 64; i++) {
    KL_TEST_EXPECT_EQ_INT(p1[i], 0xAA);
    KL_TEST_EXPECT_EQ_INT(p2[i], 0xBB);
    KL_TEST_EXPECT_EQ_INT(p3[i], 0xCC);
  }

  kl_allocator_destroy(&a);
}

void test_allocator_new_page_growth() {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 64 });

  uint8_t* p1 = kl_allocator_malloc(&a, 60);
  KL_TEST_EXPECT_EQ_INT(a.pages.length, 1);
  uint8_t* p2 = kl_allocator_malloc(&a, 60);
  KL_TEST_EXPECT_EQ_INT(a.pages.length, 2);
  uint8_t* p3 = kl_allocator_malloc(&a, 60);
  KL_TEST_EXPECT_EQ_INT(a.pages.length, 3);
  KL_TEST_ASSERT(p1 != p2 && p2 != p3 && p1 != p3, "page-spanning ptrs distinct");

  // Each independent page is its own zeroed buffer.
  memset(p1, 0x11, 60);
  memset(p2, 0x22, 60);
  memset(p3, 0x33, 60);
  for (int i = 0; i < 60; i++) {
    KL_TEST_EXPECT_EQ_INT(p1[i], 0x11);
    KL_TEST_EXPECT_EQ_INT(p2[i], 0x22);
    KL_TEST_EXPECT_EQ_INT(p3[i], 0x33);
  }

  kl_allocator_destroy(&a);
}

void test_allocator_large_alloc_grows_page() {
  // A single allocation larger than the initial capacity must succeed:
  // new_page() takes max(size, current_capacity).
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 64 });

  uint8_t* p = kl_allocator_malloc(&a, 4096);
  KL_TEST_ASSERT(p != 0, "large alloc returns non-null");
  memset(p, 0x77, 4096);
  for (int i = 0; i < 4096; i++) KL_TEST_EXPECT_EQ_INT(p[i], 0x77);

  kl_allocator_destroy(&a);
}

void test_allocator_malloc_aligned() {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 4096 });

  size_t alignments[] = { 1, 2, 4, 8, 16, 32, 64, 128 };
  for (size_t i = 0; i < KL_ARRAY_LEN(alignments); i++) {
    // Bump bytes_written by an odd amount so the next alloc needs real padding.
    kl_allocator_malloc(&a, 1);
    uint8_t* p = kl_allocator_malloc_aligned(&a, 64, alignments[i]);
    KL_TEST_ASSERT(p != 0, "aligned alloc non-null");
    KL_TEST_EXPECT_EQ_INT((int)((uintptr_t)p % alignments[i]), 0);
    // The returned buffer must be writable for its full requested size.
    memset(p, (int)(0xA0 + i), 64);
    for (int j = 0; j < 64; j++) KL_TEST_EXPECT_EQ_INT(p[j], (uint8_t)(0xA0 + i));
  }

  kl_allocator_destroy(&a);
}

void test_allocator_strdup() {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 1024 });

  const char* src = "hello world";
  int64_t n = (int64_t)strlen(src);
  char* copy = kl_allocator_strdup(&a, src, n);
  KL_TEST_ASSERT(copy != 0, "strdup non-null");
  KL_TEST_ASSERT((const char*)copy != src, "strdup returns a fresh buffer");
  KL_TEST_EXPECT_EQ_INT(strcmp(copy, src), 0);
  KL_TEST_EXPECT_EQ_INT(copy[n], 0);

  kl_allocator_destroy(&a);
}

void test_allocator_sprintf() {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 1024 });

  char* s = kl_allocator_sprintf(&a, "x=%d y=%s z=%d", 42, "ok", -7);
  KL_TEST_ASSERT(s != 0, "sprintf non-null");
  KL_TEST_EXPECT_EQ_INT(strcmp(s, "x=42 y=ok z=-7"), 0);

  kl_allocator_destroy(&a);
}

void test_allocator_reset_zeros_and_keeps_pages() {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 1024 });

  uint8_t* p = kl_allocator_malloc(&a, 128);
  memset(p, 0xFF, 128);
  int64_t pages_before = a.pages.length;

  kl_allocator_reset(&a);
  // bytes_written usage was within capacity, so pages must survive.
  KL_TEST_EXPECT_EQ_INT(a.pages.length, pages_before);
  for (int64_t i = 0; i < a.pages.length; i++) {
    KL_TEST_EXPECT_EQ_INT(a.pages.items[i].bytes_written, 0);
  }

  // A subsequent alloc must observe zeroed memory.
  uint8_t* q = kl_allocator_malloc(&a, 128);
  for (int i = 0; i < 128; i++) KL_TEST_EXPECT_EQ_INT(q[i], 0);

  kl_allocator_destroy(&a);
}

void test_allocator_reset_grows_capacity() {
  // Total usage exceeding current_capacity must cause reset() to bump it
  // and drop the existing pages.
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 64 });
  int64_t init_capacity = a.current_capacity;

  kl_allocator_malloc(&a, 60);
  kl_allocator_malloc(&a, 60);
  kl_allocator_malloc(&a, 60);
  KL_TEST_ASSERT(a.pages.length >= 2, "multiple pages allocated");

  kl_allocator_reset(&a);
  KL_TEST_ASSERT(a.current_capacity > init_capacity, "capacity grew on reset");
  KL_TEST_EXPECT_EQ_INT(a.pages.length, 0);

  kl_allocator_destroy(&a);
}

void test_allocator_realloc_grow_preserves_data() {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 1024, .track_allocations = false });

  KL_Allocation alloc = {0};
  uint8_t* p = kl_allocator_malloc_tracked(&a, 32, &alloc);
  KL_TEST_EXPECT_EQ_INT(alloc.size, 32);
  for (int i = 0; i < 32; i++) p[i] = (uint8_t)(i + 1);

  uint8_t* q = kl_allocator_realloc(&a, 128, &alloc);
  KL_TEST_ASSERT(q != 0, "realloc non-null");
  KL_TEST_EXPECT_EQ_INT(alloc.size, 128);
  for (int i = 0; i < 32; i++) KL_TEST_EXPECT_EQ_INT(q[i], (uint8_t)(i + 1));

  // Newly grown tail is zero (the new page region was calloc'd).
  for (int i = 32; i < 128; i++) KL_TEST_EXPECT_EQ_INT(q[i], 0);

  kl_allocator_destroy(&a);
}

void test_allocator_realloc_shrink_returns_same_pointer() {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 1024, .track_allocations = false });

  KL_Allocation alloc = {0};
  uint8_t* p = kl_allocator_malloc_tracked(&a, 128, &alloc);
  for (int i = 0; i < 128; i++) p[i] = (uint8_t)(i & 0xFF);
  int64_t size_before = alloc.size;

  uint8_t* q = kl_allocator_realloc(&a, 64, &alloc);
  KL_TEST_ASSERT(q == p, "shrink-realloc returns same pointer");
  KL_TEST_EXPECT_EQ_INT(alloc.size, size_before);
  // Data is untouched in shrink path.
  for (int i = 0; i < 64; i++) KL_TEST_EXPECT_EQ_INT(q[i], (uint8_t)(i & 0xFF));

  kl_allocator_destroy(&a);
}

void test_allocator_realloc_from_zero_acts_as_malloc() {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 1024, .track_allocations = false });

  KL_Allocation alloc = {0};
  uint8_t* p = kl_allocator_realloc(&a, 64, &alloc);
  KL_TEST_ASSERT(p != 0, "realloc from zero non-null");
  KL_TEST_EXPECT_EQ_INT(alloc.size, 64);
  memset(p, 0x55, 64);
  for (int i = 0; i < 64; i++) KL_TEST_EXPECT_EQ_INT(p[i], 0x55);

  kl_allocator_destroy(&a);
}

void test_allocator_tracked_malloc_records_allocations() {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){
    .capacity = 1024,
    .tracked_allocations_capacity = 8,
    .track_allocations = true,
  });

  KL_Allocation a1 = {0};
  KL_Allocation a2 = {0};
  KL_Allocation a3 = {0};
  kl_allocator_malloc_tracked(&a, 32, &a1);
  kl_allocator_malloc_tracked(&a, 32, &a2);
  kl_allocator_malloc_tracked(&a, 32, &a3);

  KL_TEST_EXPECT_EQ_INT(a1.size, 32);
  KL_TEST_EXPECT_EQ_INT(a2.size, 32);
  KL_TEST_EXPECT_EQ_INT(a3.size, 32);
  KL_TEST_EXPECT_EQ_INT(a.allocations.length, 3);
  KL_TEST_ASSERT(a.allocations.n_allocated >= 3, "tracked capacity grew");
  // Offsets are strictly increasing within the same page.
  KL_TEST_ASSERT(a1.offset < a2.offset, "a1 before a2");
  KL_TEST_ASSERT(a2.offset < a3.offset, "a2 before a3");

  kl_allocator_destroy(&a);
}

void test_allocator_free_zeros_memory_and_reuses_slot() {
  // Capacity = 256 with two 128-byte allocs fills the page exactly, so the
  // next malloc must take the freed-slot reuse path inside find_avail_page.
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){
    .capacity = 256,
    .tracked_allocations_capacity = 8,
    .track_allocations = true,
  });

  KL_Allocation alloc1 = {0};
  KL_Allocation alloc2 = {0};
  uint8_t* p1 = kl_allocator_malloc_tracked(&a, 128, &alloc1);
  uint8_t* p2 = kl_allocator_malloc_tracked(&a, 128, &alloc2);
  KL_TEST_ASSERT(p1 != 0 && p2 != 0, "both allocs succeed");
  memset(p1, 0xAB, 128);
  memset(p2, 0xCD, 128);

  kl_allocator_free_allocation(&a, alloc1);
  KL_TEST_EXPECT_EQ_INT(a.allocations.n_freed, 1);
  for (int i = 0; i < 128; i++) KL_TEST_EXPECT_EQ_INT(p1[i], 0);
  // The neighboring live allocation is untouched.
  for (int i = 0; i < 128; i++) KL_TEST_EXPECT_EQ_INT(p2[i], 0xCD);

  uint8_t* p3 = kl_allocator_malloc(&a, 128);
  KL_TEST_ASSERT(p3 == p1, "next alloc reuses the freed slot");
  KL_TEST_EXPECT_EQ_INT(a.allocations.n_freed, 0);

  kl_allocator_destroy(&a);
}

void test_allocator_free_reuses_slot_aligned() {
  // An aligned request must be able to reuse a freed block of the same user
  // size. Freed records store the user size, so the freed-slot match must
  // compare against it — matching against the bump footprint (size +
  // alignment slack) makes same-size recycling impossible for aligned allocs.
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){
    .capacity = 192,
    .tracked_allocations_capacity = 8,
    .track_allocations = true,
  });
  KL_TEST_EXPECT_EQ_INT(a.allocations.n_reused, 0);

  KL_Allocation alloc1 = {0};
  KL_Allocation alloc2 = {0};
  uint8_t* p1 = kl_allocator_malloc_tracked_aligned(&a, 128, 16, &alloc1);
  uint8_t* p2 = kl_allocator_malloc_tracked_aligned(&a, 48, 16, &alloc2);
  KL_TEST_ASSERT(p1 != 0 && p2 != 0, "both allocs succeed");

  kl_allocator_free_allocation(&a, alloc1);
  KL_TEST_EXPECT_EQ_INT(a.allocations.n_freed, 1);

  uint8_t* p3 = kl_allocator_malloc_aligned(&a, 128, 16);
  KL_TEST_ASSERT(p3 == p1, "same-size aligned alloc reuses the freed slot");
  KL_TEST_ASSERT(((uintptr_t)p3 % 16) == 0, "reused pointer keeps alignment");
  KL_TEST_EXPECT_EQ_INT(a.allocations.n_freed, 0);
  KL_TEST_EXPECT_EQ_INT(a.allocations.n_reused, 1);
  KL_TEST_EXPECT_EQ_INT(a.pages.length, 1);

  kl_allocator_destroy(&a);
}

void test_allocator_reuses_freed_slot_before_bump() {
  // Free/alloc churn must reuse freed slots even while the page still has
  // bump space, otherwise both bytes_written and the tracked-allocations
  // list grow without bound. Best-fit: the smallest fitting freed slot wins.
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){
    .capacity = 4096,
    .tracked_allocations_capacity = 8,
    .track_allocations = true,
  });

  KL_Allocation big = {0};
  KL_Allocation small = {0};
  uint8_t* p_big = kl_allocator_malloc_tracked(&a, 256, &big);
  uint8_t* p_small = kl_allocator_malloc_tracked(&a, 64, &small);
  KL_TEST_ASSERT(p_big != 0 && p_small != 0, "both allocs succeed");
  int64_t bytes_written = a.pages.items[0].bytes_written;

  kl_allocator_free_allocation(&a, big);
  kl_allocator_free_allocation(&a, small);
  KL_TEST_EXPECT_EQ_INT(a.allocations.n_freed, 2);

  // Fits both freed slots; best-fit must pick the 64-byte one, not the
  // 256-byte one and not fresh bump space.
  uint8_t* p1 = kl_allocator_malloc(&a, 64);
  KL_TEST_ASSERT(p1 == p_small, "best-fit reuses the smallest fitting slot");
  uint8_t* p2 = kl_allocator_malloc(&a, 200);
  KL_TEST_ASSERT(p2 == p_big, "larger request reuses the larger slot");

  KL_TEST_EXPECT_EQ_INT(a.allocations.n_freed, 0);
  KL_TEST_EXPECT_EQ_INT(a.allocations.n_reused, 2);
  KL_TEST_EXPECT_EQ_INT(a.allocations.length, 2);
  KL_TEST_EXPECT_EQ_INT((int)a.pages.items[0].bytes_written, (int)bytes_written);

  kl_allocator_destroy(&a);
}

void test_allocator_reused_handle_can_be_freed() {
  // A handle returned by the reuse path must be freeable again. Copying the
  // slot into the caller's handle before clearing its freed flag poisons the
  // handle: the next free is rejected, the block leaks as live, and a steady
  // free/alloc churn (per-frame text updates) grows the tracking list and
  // bytes_written forever while n_freed stays pinned.
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){
    .capacity = 4096,
    .tracked_allocations_capacity = 8,
    .track_allocations = true,
  });

  KL_Allocation alloc = {0};
  uint8_t* p0 = kl_allocator_malloc_tracked(&a, 128, &alloc);
  KL_TEST_ASSERT(p0 != 0, "initial alloc succeeds");

  for (int i = 0; i < 32; i++) {
    kl_allocator_free_allocation(&a, alloc);
    KL_TEST_EXPECT_EQ_INT(a.allocations.n_freed, 1);
    uint8_t* p = kl_allocator_malloc_tracked(&a, 128, &alloc);
    KL_TEST_ASSERT(p == p0, "every cycle reuses the same block");
    KL_TEST_ASSERT(alloc.freed == false, "returned handle is not poisoned");
    KL_TEST_EXPECT_EQ_INT(a.allocations.n_freed, 0);
  }

  KL_TEST_EXPECT_EQ_INT(a.allocations.n_reused, 32);
  KL_TEST_EXPECT_EQ_INT(a.allocations.length, 1);
  KL_TEST_EXPECT_EQ_INT(a.pages.length, 1);

  kl_allocator_destroy(&a);
}

void test_allocator_save_restore_from_empty() {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 1024 });

  KL_AllocatorMark mark = kl_allocator_save(&a);
  kl_allocator_malloc(&a, 64);
  kl_allocator_malloc(&a, 64);
  KL_TEST_ASSERT(a.pages.length >= 1, "page allocated after save");

  kl_allocator_restore(&a, mark);
  KL_TEST_EXPECT_EQ_INT(a.pages.length, 0);
  KL_TEST_EXPECT_EQ_INT(a.allocations.length, 0);

  kl_allocator_destroy(&a);
}

void test_allocator_save_restore_preserves_prior_data() {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 1024 });

  uint8_t* persistent = kl_allocator_malloc(&a, 64);
  memset(persistent, 0xAA, 64);
  int64_t pages_before = a.pages.length;

  KL_AllocatorMark mark = kl_allocator_save(&a);
  uint8_t* tmp = kl_allocator_malloc(&a, 128);
  memset(tmp, 0xBB, 128);

  kl_allocator_restore(&a, mark);
  KL_TEST_EXPECT_EQ_INT(a.pages.length, pages_before);
  for (int i = 0; i < 64; i++) KL_TEST_EXPECT_EQ_INT(persistent[i], 0xAA);
  // The region used after save was zeroed by restore.
  for (int i = 0; i < 128; i++) KL_TEST_EXPECT_EQ_INT(tmp[i], 0);

  kl_allocator_destroy(&a);
}

void test_allocator_save_restore_drops_new_pages() {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 64 });
  kl_allocator_malloc(&a, 60);
  int64_t pages_before = a.pages.length;

  KL_AllocatorMark mark = kl_allocator_save(&a);
  // Each of these overflows and creates a new page.
  kl_allocator_malloc(&a, 60);
  kl_allocator_malloc(&a, 60);
  KL_TEST_ASSERT(a.pages.length > pages_before, "extra pages created after save");

  kl_allocator_restore(&a, mark);
  KL_TEST_EXPECT_EQ_INT(a.pages.length, pages_before);

  kl_allocator_destroy(&a);
}

void test_allocator_destroy_all_pages_keeps_allocator_usable() {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 1024 });
  kl_allocator_malloc(&a, 128);
  KL_TEST_ASSERT(a.pages.length >= 1, "a page exists");

  kl_allocator_destroy_all_pages(&a);
  KL_TEST_EXPECT_EQ_INT(a.pages.length, 0);
  KL_TEST_ASSERT(a.pages.items == 0, "pages buffer released");
  KL_TEST_ASSERT(a.initialized == true, "allocator stays initialized");

  uint8_t* p = kl_allocator_malloc(&a, 64);
  KL_TEST_ASSERT(p != 0, "allocator still usable");
  memset(p, 0x99, 64);
  for (int i = 0; i < 64; i++) KL_TEST_EXPECT_EQ_INT(p[i], 0x99);

  kl_allocator_destroy(&a);
}

void test_allocator_many_small_allocs() {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 1024 });

  enum { N = 500, SZ = 16 };
  uint8_t* ptrs[N];
  for (int i = 0; i < N; i++) {
    ptrs[i] = kl_allocator_malloc(&a, SZ);
    KL_TEST_ASSERT(ptrs[i] != 0, "small alloc non-null");
    memset(ptrs[i], (uint8_t)(i & 0xFF), SZ);
  }
  // No earlier allocation was clobbered by later ones.
  for (int i = 0; i < N; i++) {
    for (int j = 0; j < SZ; j++) {
      KL_TEST_EXPECT_EQ_INT(ptrs[i][j], (uint8_t)(i & 0xFF));
    }
  }

  kl_allocator_destroy(&a);
}

void test_allocator_strdup_in_reused_page() {
  // After reset, the page is zeroed; strdup relies on that for the
  // trailing null terminator. Confirm round-tripping a string works.
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 1024 });
  uint8_t* junk = kl_allocator_malloc(&a, 256);
  memset(junk, 0xFF, 256);
  kl_allocator_reset(&a);

  const char* src = "abcdefgh";
  char* copy = kl_allocator_strdup(&a, src, (int64_t)strlen(src));
  KL_TEST_EXPECT_EQ_INT(strcmp(copy, src), 0);

  kl_allocator_destroy(&a);
}


/* ===================== tracked-realloc tests ===================== */

void test_allocator_tracked_realloc_grow_preserves_data() {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){
    .capacity = 1024,
    .track_allocations = true,
  });

  KL_Allocation alloc = {0};
  uint8_t* p = kl_allocator_malloc_tracked(&a, 32, &alloc);
  for (int i = 0; i < 32; i++) p[i] = (uint8_t)(i + 1);

  uint8_t* q = kl_allocator_realloc(&a, 128, &alloc);
  KL_TEST_ASSERT(q != 0, "tracked realloc grow non-null");
  KL_TEST_EXPECT_EQ_INT(alloc.size, 128);
  for (int i = 0; i < 32; i++) KL_TEST_EXPECT_EQ_INT(q[i], (uint8_t)(i + 1));
  // Newly extended tail is zero.
  for (int i = 32; i < 128; i++) KL_TEST_EXPECT_EQ_INT(q[i], 0);

  kl_allocator_destroy(&a);
}

void test_allocator_tracked_realloc_grow_marks_old_freed() {
  // The old slot's tracker entry must be marked freed (not removed) so the
  // space is reusable. n_freed bumps by 1; the live count gains a new entry.
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){
    .capacity = 1024,
    .track_allocations = true,
  });

  KL_Allocation alloc = {0};
  kl_allocator_malloc_tracked(&a, 32, &alloc);
  KL_TEST_EXPECT_EQ_INT(a.allocations.length, 1);
  KL_TEST_EXPECT_EQ_INT(a.allocations.n_freed, 0);

  kl_allocator_realloc(&a, 128, &alloc);
  KL_TEST_EXPECT_EQ_INT(a.allocations.length, 2);
  KL_TEST_EXPECT_EQ_INT(a.allocations.n_freed, 1);

  kl_allocator_destroy(&a);
}

void test_allocator_tracked_realloc_grow_zeros_old_region() {
  // Marking the old slot freed implies its bytes are zeroed (free_allocation
  // contract). Reading through the old pointer after realloc is UB by API
  // contract, but we can inspect the page directly.
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){
    .capacity = 1024,
    .track_allocations = true,
  });

  KL_Allocation alloc = {0};
  uint8_t* p_old = kl_allocator_malloc_tracked(&a, 32, &alloc);
  for (int i = 0; i < 32; i++) p_old[i] = 0xAB;

  kl_allocator_realloc(&a, 128, &alloc);
  for (int i = 0; i < 32; i++) KL_TEST_EXPECT_EQ_INT(p_old[i], 0);

  kl_allocator_destroy(&a);
}

void test_allocator_tracked_realloc_old_slot_reusable() {
  // After a realloc-grow, the original slot becomes freed; a fresh malloc
  // small enough to fit must reuse it.
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){
    .capacity = 256,
    .track_allocations = true,
  });

  // Two allocations to fill the page exactly; reuse path is the only option
  // for the next allocation of size <= 64.
  KL_Allocation a1 = {0};
  KL_Allocation a2 = {0};
  uint8_t* p1 = kl_allocator_malloc_tracked(&a, 64, &a1);
  kl_allocator_malloc_tracked(&a, 64, &a2);

  // Pad the rest so the page is full.
  KL_Allocation pad = {0};
  kl_allocator_malloc_tracked(&a, 128, &pad);

  // Now grow a1: the new region must move to a new page since the first
  // is full, and a1's old slot becomes freed.
  kl_allocator_realloc(&a, 200, &a1);
  KL_TEST_ASSERT(a.allocations.n_freed >= 1, "old slot is freed");

  // A 64-byte malloc must reuse the freed slot at the original p1 address.
  KL_Allocation a3 = {0};
  uint8_t* p3 = kl_allocator_malloc_tracked(&a, 64, &a3);
  KL_TEST_ASSERT(p3 == p1, "new alloc reuses old freed slot");
  KL_TEST_EXPECT_EQ_INT(a.allocations.n_freed, 0);

  kl_allocator_destroy(&a);
}

void test_allocator_tracked_realloc_new_region_is_freeable() {
  // The post-realloc allocation handle must point at a tracked entry: freeing
  // it must succeed (returns 1 / bumps n_freed).
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){
    .capacity = 1024,
    .track_allocations = true,
  });

  KL_Allocation alloc = {0};
  kl_allocator_malloc_tracked(&a, 32, &alloc);
  kl_allocator_realloc(&a, 128, &alloc);

  int64_t freed_before = a.allocations.n_freed; // already 1 (old region)
  kl_allocator_free_allocation(&a, alloc);
  KL_TEST_EXPECT_EQ_INT(a.allocations.n_freed, freed_before + 1);

  kl_allocator_destroy(&a);
}

void test_allocator_tracked_realloc_shrink_preserves_pointer_and_tracking() {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){
    .capacity = 1024,
    .track_allocations = true,
  });

  KL_Allocation alloc = {0};
  uint8_t* p = kl_allocator_malloc_tracked(&a, 128, &alloc);
  int64_t size_before = alloc.size;
  for (int i = 0; i < 128; i++) p[i] = (uint8_t)(i & 0xFF);

  uint8_t* q = kl_allocator_realloc(&a, 64, &alloc);
  KL_TEST_ASSERT(q == p, "shrink returns same pointer");
  KL_TEST_EXPECT_EQ_INT(alloc.size, size_before);
  // The single tracker entry remains live and untouched.
  KL_TEST_EXPECT_EQ_INT(a.allocations.length, 1);
  KL_TEST_EXPECT_EQ_INT(a.allocations.n_freed, 0);
  // Data within the requested shrink range is preserved.
  for (int i = 0; i < 64; i++) KL_TEST_EXPECT_EQ_INT(q[i], (uint8_t)(i & 0xFF));

  // Tracker entry still matches the handle — free succeeds.
  kl_allocator_free_allocation(&a, alloc);
  KL_TEST_EXPECT_EQ_INT(a.allocations.n_freed, 1);

  kl_allocator_destroy(&a);
}

void test_allocator_tracked_realloc_from_zero_tracks_new_region() {
  // realloc called with size 0 acts as malloc. With tracking on, the new
  // region must be tracked — previously it was silently untracked.
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){
    .capacity = 1024,
    .track_allocations = true,
  });

  KL_Allocation alloc = {0};
  uint8_t* p = kl_allocator_realloc(&a, 64, &alloc);
  KL_TEST_ASSERT(p != 0, "realloc from zero non-null");
  KL_TEST_EXPECT_EQ_INT(alloc.size, 64);
  KL_TEST_EXPECT_EQ_INT(a.allocations.length, 1);
  KL_TEST_EXPECT_EQ_INT(a.allocations.n_freed, 0);

  kl_allocator_free_allocation(&a, alloc);
  KL_TEST_EXPECT_EQ_INT(a.allocations.n_freed, 1);

  kl_allocator_destroy(&a);
}

void test_allocator_tracked_realloc_aligned() {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){
    .capacity = 4096,
    .track_allocations = true,
  });

  KL_Allocation alloc = {0};
  uint8_t* p = kl_allocator_malloc_tracked_aligned(&a, 16, 32, &alloc);
  KL_TEST_ASSERT(((uintptr_t)p % 32) == 0, "initial alloc aligned");
  for (int i = 0; i < 16; i++) p[i] = (uint8_t)(0x10 + i);

  uint8_t* q = kl_allocator_realloc_aligned(&a, 96, 32, &alloc);
  KL_TEST_ASSERT(q != 0, "aligned grow non-null");
  KL_TEST_ASSERT(((uintptr_t)q % 32) == 0, "post-grow pointer still aligned");
  for (int i = 0; i < 16; i++) KL_TEST_EXPECT_EQ_INT(q[i], (uint8_t)(0x10 + i));

  // Both old (freed) and new (live) tracker entries exist.
  KL_TEST_EXPECT_EQ_INT(a.allocations.length, 2);
  KL_TEST_EXPECT_EQ_INT(a.allocations.n_freed, 1);

  kl_allocator_destroy(&a);
}

void test_allocator_tracked_realloc_chain() {
  // Repeated realloc-grows: each old slot accumulates as freed; the
  // live region is always the most recent.
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){
    .capacity = 4096,
    .track_allocations = true,
  });

  KL_Allocation alloc = {0};
  uint8_t* p = kl_allocator_malloc_tracked(&a, 16, &alloc);
  for (int i = 0; i < 16; i++) p[i] = (uint8_t)(i + 1);

  p = kl_allocator_realloc(&a, 32, &alloc);
  p = kl_allocator_realloc(&a, 64, &alloc);
  p = kl_allocator_realloc(&a, 128, &alloc);
  p = kl_allocator_realloc(&a, 256, &alloc);

  KL_TEST_EXPECT_EQ_INT(alloc.size, 256);
  for (int i = 0; i < 16; i++) KL_TEST_EXPECT_EQ_INT(p[i], (uint8_t)(i + 1));

  // 4 grows produced 4 freed slots; the final region is the one live entry.
  KL_TEST_EXPECT_EQ_INT(a.allocations.length, 5);
  KL_TEST_EXPECT_EQ_INT(a.allocations.n_freed, 4);

  // Final live region is freeable.
  kl_allocator_free_allocation(&a, alloc);
  KL_TEST_EXPECT_EQ_INT(a.allocations.n_freed, 5);

  kl_allocator_destroy(&a);
}

void test_allocator_tracked_realloc_reuses_pre_freed_slot() {
  // Pre-create a freed slot, then realloc into it.
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){
    .capacity = 512,
    .track_allocations = true,
  });

  // The slot we'll free and want reclaimed by the realloc.
  KL_Allocation big = {0};
  uint8_t* p_big = kl_allocator_malloc_tracked(&a, 256, &big);

  // A small alloc we'll grow.
  KL_Allocation small = {0};
  uint8_t* p_small = kl_allocator_malloc_tracked(&a, 32, &small);
  for (int i = 0; i < 32; i++) p_small[i] = (uint8_t)(0xC0 + i);

  // Fill the rest so the bump path can't accommodate growth of `small`.
  KL_Allocation pad = {0};
  kl_allocator_malloc_tracked(&a, 200, &pad);

  // Free `big` to create a freed slot at p_big with size 256.
  kl_allocator_free_allocation(&a, big);
  KL_TEST_EXPECT_EQ_INT(a.allocations.n_freed, 1);

  // Grow `small` to 200 (fits inside big's slot). The new region must land
  // at p_big since the page is otherwise full.
  uint8_t* p_grown = kl_allocator_realloc(&a, 200, &small);
  KL_TEST_ASSERT(p_grown == p_big, "realloc reused the pre-existing freed slot");
  for (int i = 0; i < 32; i++) KL_TEST_EXPECT_EQ_INT(p_grown[i], (uint8_t)(0xC0 + i));

  kl_allocator_destroy(&a);
}

void test_allocator_tracked_realloc_many_independent_handles() {
  // Many handles, each grown once. Each handle's *final* allocation must be
  // freeable; total n_freed should reflect old + final frees.
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){
    .capacity = 8192,
    .track_allocations = true,
  });

  enum { N = 20 };
  KL_Allocation handles[N];
  for (int i = 0; i < N; i++) {
    handles[i] = (KL_Allocation){0};
    uint8_t* p = kl_allocator_malloc_tracked(&a, 32, &handles[i]);
    for (int j = 0; j < 32; j++) p[j] = (uint8_t)(i + 1);
  }
  KL_TEST_EXPECT_EQ_INT(a.allocations.length, N);
  KL_TEST_EXPECT_EQ_INT(a.allocations.n_freed, 0);

  // Grow every other one to 128. Each grow leaves one freed slot behind.
  for (int i = 0; i < N; i += 2) {
    uint8_t* g = kl_allocator_realloc(&a, 128, &handles[i]);
    // Original data still intact.
    for (int j = 0; j < 32; j++) KL_TEST_EXPECT_EQ_INT(g[j], (uint8_t)(i + 1));
  }
  KL_TEST_EXPECT_EQ_INT(a.allocations.n_freed, N / 2);

  // Free every handle. Each must match a live tracker entry.
  for (int i = 0; i < N; i++) {
    int64_t before = a.allocations.n_freed;
    kl_allocator_free_allocation(&a, handles[i]);
    KL_TEST_EXPECT_EQ_INT(a.allocations.n_freed, before + 1);
  }

  kl_allocator_destroy(&a);
}
