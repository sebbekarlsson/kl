#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <test/framework.h>
#include <kl/macros.h>
#include <kl/string/string.h>
#include <kl/memory/allocator.h>
#include <kl/templates/list/int64_list.h>

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


/* ===================== tracked-realloc tests ===================== */

static void test_allocator_tracked_realloc_grow_preserves_data() {
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

static void test_allocator_tracked_realloc_grow_marks_old_freed() {
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

static void test_allocator_tracked_realloc_grow_zeros_old_region() {
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

static void test_allocator_tracked_realloc_old_slot_reusable() {
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

static void test_allocator_tracked_realloc_new_region_is_freeable() {
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

static void test_allocator_tracked_realloc_shrink_preserves_pointer_and_tracking() {
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

static void test_allocator_tracked_realloc_from_zero_tracks_new_region() {
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

static void test_allocator_tracked_realloc_aligned() {
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

static void test_allocator_tracked_realloc_chain() {
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

static void test_allocator_tracked_realloc_reuses_pre_freed_slot() {
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

static void test_allocator_tracked_realloc_many_independent_handles() {
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

/* ===================== int64_list tests ===================== */

static void test_int64_list_init_destroy() {
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

static void test_int64_list_double_init_is_noop() {
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 8 });
  int64_t cap_before = list.current_capacity;
  // Re-initing an already-initialized list must be a no-op.
  KL_TEST_EXPECT_EQ_INT(kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 9999 }), 1);
  KL_TEST_EXPECT_EQ_INT(list.current_capacity, cap_before);
  kl_int64_list_destroy(&list);
}

static void test_int64_list_push_basic() {
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });

  kl_int64_list_push(&list, 42);
  KL_TEST_EXPECT_EQ_INT(list.length, 1);
  KL_TEST_ASSERT(list.items != 0, "items buffer allocated on first push");
  KL_TEST_ASSERT(list.n_slots_allocated >= 1, "at least one slot allocated");
  KL_TEST_EXPECT_EQ_INT((int)list.items[0], 42);

  kl_int64_list_destroy(&list);
}

static void test_int64_list_push_many_preserves_order() {
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

static void test_int64_list_push_grows_capacity() {
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

static void test_int64_list_grow_increases_slots() {
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

static void test_int64_list_grow_zeros_new_slots() {
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

static void test_int64_list_pop_basic() {
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

static void test_int64_list_pop_lifo_order() {
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

static void test_int64_list_pop_with_null_out() {
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });
  kl_int64_list_push(&list, 7);
  kl_int64_list_push(&list, 8);

  KL_TEST_EXPECT_EQ_INT(kl_int64_list_pop(&list, 0), 1);
  KL_TEST_EXPECT_EQ_INT(list.length, 1);
  KL_TEST_EXPECT_EQ_INT((int)list.items[0], 7);

  kl_int64_list_destroy(&list);
}

static void test_int64_list_pop_empty_returns_zero() {
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

static void test_int64_list_pop_at_first() {
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

static void test_int64_list_pop_at_middle() {
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

static void test_int64_list_pop_at_last() {
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

static void test_int64_list_pop_at_empty_returns_zero() {
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });
  // Never pushed: items is null. Must return 0 without asserting.
  int64_t out = 999;
  KL_TEST_EXPECT_EQ_INT(kl_int64_list_pop_at(&list, 0, &out), 0);
  KL_TEST_EXPECT_EQ_INT((int)out, 999);
  kl_int64_list_destroy(&list);
}

static void test_int64_list_push_start_basic() {
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });

  kl_int64_list_push_start(&list, 99);
  KL_TEST_EXPECT_EQ_INT(list.length, 1);
  KL_TEST_EXPECT_EQ_INT((int)list.items[0], 99);

  kl_int64_list_destroy(&list);
}

static void test_int64_list_push_start_reverses_insertion_order() {
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

static void test_int64_list_push_start_after_push() {
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

static void test_int64_list_push_start_grows_when_needed() {
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

static void test_int64_list_includes_found() {
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

static void test_int64_list_includes_not_found() {
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

static void test_int64_list_includes_empty() {
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });
  KL_TEST_ASSERT(kl_int64_list_includes(&list, 0) == false, "empty list contains nothing");
  KL_TEST_ASSERT(kl_int64_list_includes(&list, 42) == false, "empty list contains nothing");
  kl_int64_list_destroy(&list);
}

static void test_int64_list_includes_after_pop() {
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

static void test_int64_list_reset_clears_length() {
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

static void test_int64_list_reset_zeros_items() {
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });
  for (int i = 0; i < 8; i++) kl_int64_list_push(&list, 1000 + i);

  kl_int64_list_reset(&list);
  for (int64_t i = 0; i < list.n_slots_allocated; i++) {
    KL_TEST_EXPECT_EQ_INT((int)list.items[i], 0);
  }

  kl_int64_list_destroy(&list);
}

static void test_int64_list_reset_allows_reuse() {
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });
  for (int i = 0; i < 5; i++) kl_int64_list_push(&list, i);
  kl_int64_list_reset(&list);

  for (int i = 0; i < 7; i++) kl_int64_list_push(&list, 100 + i);
  KL_TEST_EXPECT_EQ_INT(list.length, 7);
  for (int i = 0; i < 7; i++) KL_TEST_EXPECT_EQ_INT((int)list.items[i], 100 + i);

  kl_int64_list_destroy(&list);
}

static void test_int64_list_reset_on_empty_is_safe() {
  // Reset before any push (items == 0) must not segfault.
  KL_int64_List list = {0};
  kl_int64_list_init(&list, (KL_ListConfig){ .capacity = 4 });
  kl_int64_list_reset(&list);
  KL_TEST_EXPECT_EQ_INT(list.length, 0);
  KL_TEST_ASSERT(list.items == 0, "no buffer was allocated yet");
  kl_int64_list_destroy(&list);
}

static void test_int64_list_with_arena_allocator() {
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

static void test_int64_list_arena_grow_path() {
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

static void test_int64_list_push_pop_interleaved() {
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

static void test_int64_list_handles_negative_and_extreme_values() {
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

static void test_int64_list_stress_many_elements() {
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

static void test_int64_list_pop_at_drains_from_front() {
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

    KL_TEST_ENTRY(test_allocator_tracked_realloc_grow_preserves_data),
    KL_TEST_ENTRY(test_allocator_tracked_realloc_grow_marks_old_freed),
    KL_TEST_ENTRY(test_allocator_tracked_realloc_grow_zeros_old_region),
    KL_TEST_ENTRY(test_allocator_tracked_realloc_old_slot_reusable),
    KL_TEST_ENTRY(test_allocator_tracked_realloc_new_region_is_freeable),
    KL_TEST_ENTRY(test_allocator_tracked_realloc_shrink_preserves_pointer_and_tracking),
    KL_TEST_ENTRY(test_allocator_tracked_realloc_from_zero_tracks_new_region),
    KL_TEST_ENTRY(test_allocator_tracked_realloc_aligned),
    KL_TEST_ENTRY(test_allocator_tracked_realloc_chain),
    KL_TEST_ENTRY(test_allocator_tracked_realloc_reuses_pre_freed_slot),
    KL_TEST_ENTRY(test_allocator_tracked_realloc_many_independent_handles),

    KL_TEST_ENTRY(test_int64_list_init_destroy),
    KL_TEST_ENTRY(test_int64_list_double_init_is_noop),
    KL_TEST_ENTRY(test_int64_list_push_basic),
    KL_TEST_ENTRY(test_int64_list_push_many_preserves_order),
    KL_TEST_ENTRY(test_int64_list_push_grows_capacity),
    KL_TEST_ENTRY(test_int64_list_grow_increases_slots),
    KL_TEST_ENTRY(test_int64_list_grow_zeros_new_slots),
    KL_TEST_ENTRY(test_int64_list_pop_basic),
    KL_TEST_ENTRY(test_int64_list_pop_lifo_order),
    KL_TEST_ENTRY(test_int64_list_pop_with_null_out),
    KL_TEST_ENTRY(test_int64_list_pop_empty_returns_zero),
    KL_TEST_ENTRY(test_int64_list_pop_at_first),
    KL_TEST_ENTRY(test_int64_list_pop_at_middle),
    KL_TEST_ENTRY(test_int64_list_pop_at_last),
    KL_TEST_ENTRY(test_int64_list_pop_at_empty_returns_zero),
    KL_TEST_ENTRY(test_int64_list_push_start_basic),
    KL_TEST_ENTRY(test_int64_list_push_start_reverses_insertion_order),
    KL_TEST_ENTRY(test_int64_list_push_start_after_push),
    KL_TEST_ENTRY(test_int64_list_push_start_grows_when_needed),
    KL_TEST_ENTRY(test_int64_list_includes_found),
    KL_TEST_ENTRY(test_int64_list_includes_not_found),
    KL_TEST_ENTRY(test_int64_list_includes_empty),
    KL_TEST_ENTRY(test_int64_list_includes_after_pop),
    KL_TEST_ENTRY(test_int64_list_reset_clears_length),
    KL_TEST_ENTRY(test_int64_list_reset_zeros_items),
    KL_TEST_ENTRY(test_int64_list_reset_allows_reuse),
    KL_TEST_ENTRY(test_int64_list_reset_on_empty_is_safe),
    KL_TEST_ENTRY(test_int64_list_with_arena_allocator),
    KL_TEST_ENTRY(test_int64_list_arena_grow_path),
    KL_TEST_ENTRY(test_int64_list_push_pop_interleaved),
    KL_TEST_ENTRY(test_int64_list_handles_negative_and_extreme_values),
    KL_TEST_ENTRY(test_int64_list_stress_many_elements),
    KL_TEST_ENTRY(test_int64_list_pop_at_drains_from_front),
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
