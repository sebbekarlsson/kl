#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <test/framework.h>
#include <kl/macros.h>
#include <kl/string/string.h>
#include <kl/memory/allocator.h>

static KL_StrConst get_arg(int argc, char* argv[], KL_StrConst key, bool required) {
  KL_StrConst ckey = kl_str_const_remove_prefix(key, KL_STR_CONST_LIT("--"));
  ckey = kl_str_const_remove_prefix(ckey, KL_STR_CONST_LIT("-"));

  for (int i = 0; i < argc; i++) {
    KL_StrConst arg_a = KL_STR_CONST(argv[i]);
    KL_StrConst arg_b = {0};
    if (i < (argc -1)) {
      arg_b = KL_STR_CONST(argv[i+1]);
    }
    arg_a = kl_str_const_remove_prefix(arg_a, KL_STR_CONST_LIT("--"));
    arg_a = kl_str_const_remove_prefix(arg_a, KL_STR_CONST_LIT("-"));
    if (arg_a.value != 0 && arg_a.length > 0 && kl_str_const_eq(arg_a, ckey)) {
      if (
        arg_b.value != 0 &&
        arg_b.length > 0 &&
        !kl_str_const_starts_with(arg_b, KL_STR_CONST_LIT("-"))
      ) {
        return arg_b;
      }
      return arg_a;
    }
  }

  if (required) {
    fprintf(stderr, "Missing required argument: %s\n", key.value);
    exit(1);
  }

  return (KL_StrConst){0};
}


static void test_allocator_init_destroy() {
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

static void test_allocator_double_init_is_noop() {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 128 });
  int64_t cap_after_first = a.current_capacity;
  // Calling init again on an initialized allocator must be a no-op.
  KL_TEST_EXPECT_EQ_INT(kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 9999 }), 1);
  KL_TEST_EXPECT_EQ_INT(a.current_capacity, cap_after_first);
  kl_allocator_destroy(&a);
}

static void test_allocator_basic_malloc() {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 1024 });

  uint8_t* p = kl_allocator_malloc(&a, 64);
  KL_TEST_ASSERT(p != 0, "malloc returns non-null");
  for (int i = 0; i < 64; i++) p[i] = (uint8_t)(i & 0xFF);
  for (int i = 0; i < 64; i++) KL_TEST_EXPECT_EQ_INT(p[i], (uint8_t)(i & 0xFF));
  KL_TEST_EXPECT_EQ_INT(a.pages.length, 1);

  kl_allocator_destroy(&a);
}

static void test_allocator_zero_initialized_memory() {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 256 });
  uint8_t* p = kl_allocator_malloc(&a, 128);
  for (int i = 0; i < 128; i++) KL_TEST_EXPECT_EQ_INT(p[i], 0);
  kl_allocator_destroy(&a);
}

static void test_allocator_multiple_mallocs_no_overlap() {
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

static void test_allocator_new_page_growth() {
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

static void test_allocator_large_alloc_grows_page() {
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

static void test_allocator_malloc_aligned() {
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

static void test_allocator_strdup() {
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

static void test_allocator_sprintf() {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 1024 });

  char* s = kl_allocator_sprintf(&a, "x=%d y=%s z=%d", 42, "ok", -7);
  KL_TEST_ASSERT(s != 0, "sprintf non-null");
  KL_TEST_EXPECT_EQ_INT(strcmp(s, "x=42 y=ok z=-7"), 0);

  kl_allocator_destroy(&a);
}

static void test_allocator_reset_zeros_and_keeps_pages() {
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

static void test_allocator_reset_grows_capacity() {
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

static void test_allocator_realloc_grow_preserves_data() {
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

static void test_allocator_realloc_shrink_returns_same_pointer() {
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

static void test_allocator_realloc_from_zero_acts_as_malloc() {
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

static void test_allocator_tracked_malloc_records_allocations() {
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

static void test_allocator_free_zeros_memory_and_reuses_slot() {
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

static void test_allocator_save_restore_from_empty() {
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

static void test_allocator_save_restore_preserves_prior_data() {
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

static void test_allocator_save_restore_drops_new_pages() {
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

static void test_allocator_destroy_all_pages_keeps_allocator_usable() {
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

static void test_allocator_many_small_allocs() {
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

static void test_allocator_strdup_in_reused_page() {
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


static void run_test(KL_TestEntry* entry) {
  KL_TEST_BEGIN(entry->name);
  entry->fn();
  KL_TEST_END(entry->name);
}

int main(int argc, char* argv[]) {
  KL_TestEntry tests[] = {
    KL_TEST_ENTRY(test_allocator_init_destroy),
    KL_TEST_ENTRY(test_allocator_double_init_is_noop),
    KL_TEST_ENTRY(test_allocator_basic_malloc),
    KL_TEST_ENTRY(test_allocator_zero_initialized_memory),
    KL_TEST_ENTRY(test_allocator_multiple_mallocs_no_overlap),
    KL_TEST_ENTRY(test_allocator_new_page_growth),
    KL_TEST_ENTRY(test_allocator_large_alloc_grows_page),
    KL_TEST_ENTRY(test_allocator_malloc_aligned),
    KL_TEST_ENTRY(test_allocator_strdup),
    KL_TEST_ENTRY(test_allocator_sprintf),
    KL_TEST_ENTRY(test_allocator_reset_zeros_and_keeps_pages),
    KL_TEST_ENTRY(test_allocator_reset_grows_capacity),
    KL_TEST_ENTRY(test_allocator_realloc_grow_preserves_data),
    KL_TEST_ENTRY(test_allocator_realloc_shrink_returns_same_pointer),
    KL_TEST_ENTRY(test_allocator_realloc_from_zero_acts_as_malloc),
    KL_TEST_ENTRY(test_allocator_tracked_malloc_records_allocations),
    KL_TEST_ENTRY(test_allocator_free_zeros_memory_and_reuses_slot),
    KL_TEST_ENTRY(test_allocator_save_restore_from_empty),
    KL_TEST_ENTRY(test_allocator_save_restore_preserves_prior_data),
    KL_TEST_ENTRY(test_allocator_save_restore_drops_new_pages),
    KL_TEST_ENTRY(test_allocator_destroy_all_pages_keeps_allocator_usable),
    KL_TEST_ENTRY(test_allocator_many_small_allocs),
    KL_TEST_ENTRY(test_allocator_strdup_in_reused_page),
  };

  int64_t n_tests = KL_ARRAY_LEN(tests);

  KL_StrConst test_to_run = get_arg(argc, argv, KL_STR_CONST_LIT("--test"), false);

  if (test_to_run.value != 0) {
    for (int64_t i = 0; i < n_tests; i++) {
      KL_TestEntry* entry = &tests[i];
      if (kl_str_const_eq(test_to_run, KL_STR_CONST(entry->name))) {
        run_test(entry);
        break;
      }
    }
    fprintf(stderr, "No such test: '%s'\n", test_to_run.value);
    exit(1);
  } else {
    for (int64_t i = 0; i < n_tests; i++) {
      KL_TestEntry* entry = &tests[i];
      run_test(entry);
    }
  }


  return 0;
}
