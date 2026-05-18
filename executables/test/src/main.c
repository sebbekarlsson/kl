#include <stdio.h>
#include <stdint.h>
#include <stdbool.h>
#include <string.h>
#include <test/framework.h>
#include <kl/macros.h>
#include <kl/string/string.h>
#include <kl/memory/allocator.h>
#include <kl/templates/list/int64_list.h>
#include <kl/templates/list/blob_ptr_list.h>
#include <kl/templates/hashmap/int64_hashmap.h>
#include <kl/templates/sparse_set/binary_sparse_set.h>
#include <kl/binary/blob.h>
#include <kl/io/fs.h>
#include <unistd.h>

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


static KL_HashmapConfig kl_test_int64_hashmap_default_cfg(void) {
  return (KL_HashmapConfig){
    .capacity = 16,
    .collision_capacity = 16,
    .max_key_size = sizeof(int64_t),
    .track_used_slots = false,
  };
}

static void test_int64_hashmap_init_destroy() {
  KL_int64_Hashmap map = {0};
  KL_HashmapConfig cfg = kl_test_int64_hashmap_default_cfg();
  KL_TEST_EXPECT_EQ_INT(kl_int64_hashmap_init(&map, cfg), 1);
  KL_TEST_ASSERT(map.initialized == true, "initialized flag set");
  // capacity rounds up to a power of two; 16 is already a pow2.
  KL_TEST_EXPECT_EQ_INT((int)map.capacity_pow2, 16);
  KL_TEST_EXPECT_EQ_INT((int)map.capacity_mask, 15);
  // Home slots pre-populated as part of slots.length.
  KL_TEST_EXPECT_EQ_INT((int)map.slots.length, 16);
  KL_TEST_ASSERT(map.slots.items != 0, "slot buffer allocated");
  KL_TEST_EXPECT_EQ_INT((int)map.n_collisions, 0);

  kl_int64_hashmap_destroy(&map);
  KL_TEST_ASSERT(map.initialized == false, "initialized cleared after destroy");
  KL_TEST_ASSERT(map.slots.items == 0, "slots buffer freed");
  KL_TEST_EXPECT_EQ_INT((int)map.slots.length, 0);
  KL_TEST_EXPECT_EQ_INT((int)map.slots.n_allocated, 0);
  KL_TEST_EXPECT_EQ_INT((int)map.n_collisions, 0);
}

static void test_int64_hashmap_double_init_is_noop() {
  KL_int64_Hashmap map = {0};
  KL_HashmapConfig cfg = kl_test_int64_hashmap_default_cfg();
  KL_TEST_EXPECT_EQ_INT(kl_int64_hashmap_init(&map, cfg), 1);
  int64_t pow2_after_first = map.capacity_pow2;

  // A second init on an initialized map must short-circuit and keep the
  // existing capacity, regardless of the requested capacity.
  KL_HashmapConfig cfg2 = cfg;
  cfg2.capacity = 9999;
  KL_TEST_EXPECT_EQ_INT(kl_int64_hashmap_init(&map, cfg2), 1);
  KL_TEST_EXPECT_EQ_INT((int)map.capacity_pow2, (int)pow2_after_first);

  kl_int64_hashmap_destroy(&map);
}

static void test_int64_hashmap_capacity_rounds_to_power_of_two() {
  struct { int64_t requested; int64_t pow2; } cases[] = {
    { 1, 1 }, { 2, 2 }, { 3, 4 }, { 5, 8 }, { 7, 8 }, { 8, 8 },
    { 9, 16 }, { 33, 64 }, { 100, 128 },
  };
  for (size_t i = 0; i < KL_ARRAY_LEN(cases); i++) {
    KL_int64_Hashmap map = {0};
    KL_HashmapConfig cfg = kl_test_int64_hashmap_default_cfg();
    cfg.capacity = cases[i].requested;
    kl_int64_hashmap_init(&map, cfg);
    KL_TEST_EXPECT_EQ_INT((int)map.capacity_pow2, (int)cases[i].pow2);
    KL_TEST_EXPECT_EQ_INT((int)map.capacity_mask, (int)(cases[i].pow2 - 1));
    // Home slots fill the entire pow2 region by construction.
    KL_TEST_EXPECT_EQ_INT((int)map.slots.length, (int)cases[i].pow2);
    kl_int64_hashmap_destroy(&map);
  }
}

static void test_int64_hashmap_set_get_basic() {
  KL_int64_Hashmap map = {0};
  kl_int64_hashmap_init(&map, kl_test_int64_hashmap_default_cfg());

  int64_t k1 = 1, k2 = 2, k3 = 3;
  KL_TEST_ASSERT(kl_int64_hashmap_set(&map, KL_BLOB_AUTO(k1), 100) >= 0, "set k1");
  KL_TEST_ASSERT(kl_int64_hashmap_set(&map, KL_BLOB_AUTO(k2), 200) >= 0, "set k2");
  KL_TEST_ASSERT(kl_int64_hashmap_set(&map, KL_BLOB_AUTO(k3), 300) >= 0, "set k3");

  int64_t out = 0;
  KL_TEST_EXPECT_EQ_INT(kl_int64_hashmap_get(&map, KL_BLOB_AUTO(k1), &out), 1);
  KL_TEST_EXPECT_EQ_INT((int)out, 100);
  KL_TEST_EXPECT_EQ_INT(kl_int64_hashmap_get(&map, KL_BLOB_AUTO(k2), &out), 1);
  KL_TEST_EXPECT_EQ_INT((int)out, 200);
  KL_TEST_EXPECT_EQ_INT(kl_int64_hashmap_get(&map, KL_BLOB_AUTO(k3), &out), 1);
  KL_TEST_EXPECT_EQ_INT((int)out, 300);

  kl_int64_hashmap_destroy(&map);
}

static void test_int64_hashmap_get_missing_returns_zero() {
  KL_int64_Hashmap map = {0};
  kl_int64_hashmap_init(&map, kl_test_int64_hashmap_default_cfg());

  int64_t k = 42;
  int64_t out = 0xDEADBEEF;
  // Empty map: get must report not-found and leave out untouched.
  KL_TEST_EXPECT_EQ_INT(kl_int64_hashmap_get(&map, KL_BLOB_AUTO(k), &out), 0);
  KL_TEST_EXPECT_EQ_INT((int)out, (int)0xDEADBEEF);

  // After inserting an unrelated key, a different key must still miss.
  int64_t other = 1;
  kl_int64_hashmap_set(&map, KL_BLOB_AUTO(other), 7);
  KL_TEST_EXPECT_EQ_INT(kl_int64_hashmap_get(&map, KL_BLOB_AUTO(k), &out), 0);

  kl_int64_hashmap_destroy(&map);
}

static void test_int64_hashmap_set_overwrite_updates_value() {
  KL_int64_Hashmap map = {0};
  kl_int64_hashmap_init(&map, kl_test_int64_hashmap_default_cfg());

  int64_t k = 11;
  int64_t idx_a = kl_int64_hashmap_set(&map, KL_BLOB_AUTO(k), 111);
  int64_t idx_b = kl_int64_hashmap_set(&map, KL_BLOB_AUTO(k), 999);
  // Overwrite must land in the same slot, not allocate a new one.
  KL_TEST_EXPECT_EQ_INT((int)idx_a, (int)idx_b);

  int64_t out = 0;
  KL_TEST_EXPECT_EQ_INT(kl_int64_hashmap_get(&map, KL_BLOB_AUTO(k), &out), 1);
  KL_TEST_EXPECT_EQ_INT((int)out, 999);
  // n_collisions must not increase on overwrite.
  KL_TEST_EXPECT_EQ_INT((int)map.n_collisions, 0);

  kl_int64_hashmap_destroy(&map);
}

static void test_int64_hashmap_set_returns_valid_slot_index() {
  KL_int64_Hashmap map = {0};
  kl_int64_hashmap_init(&map, kl_test_int64_hashmap_default_cfg());

  int64_t k = 5;
  int64_t idx = kl_int64_hashmap_set(&map, KL_BLOB_AUTO(k), 42);
  KL_TEST_ASSERT(idx >= 0, "slot index non-negative");
  KL_TEST_ASSERT(idx < map.slots.length, "slot index in-bounds");
  // The returned index points to the slot we just wrote.
  KL_TEST_ASSERT(map.slots.items[idx].is_set, "returned slot is set");
  KL_TEST_EXPECT_EQ_INT((int)map.slots.items[idx].value, 42);

  kl_int64_hashmap_destroy(&map);
}

static void test_int64_hashmap_has_before_and_after() {
  KL_int64_Hashmap map = {0};
  kl_int64_hashmap_init(&map, kl_test_int64_hashmap_default_cfg());

  int64_t k = 77;
  KL_TEST_EXPECT_EQ_INT((int)kl_int64_hashmap_has(&map, KL_BLOB_AUTO(k)), 0);
  kl_int64_hashmap_set(&map, KL_BLOB_AUTO(k), 1);
  KL_TEST_EXPECT_EQ_INT((int)kl_int64_hashmap_has(&map, KL_BLOB_AUTO(k)), 1);
  kl_int64_hashmap_unset(&map, KL_BLOB_AUTO(k));
  KL_TEST_EXPECT_EQ_INT((int)kl_int64_hashmap_has(&map, KL_BLOB_AUTO(k)), 0);

  kl_int64_hashmap_destroy(&map);
}

static void test_int64_hashmap_unset_removes_entry() {
  KL_int64_Hashmap map = {0};
  kl_int64_hashmap_init(&map, kl_test_int64_hashmap_default_cfg());

  int64_t k = 7;
  kl_int64_hashmap_set(&map, KL_BLOB_AUTO(k), 333);
  KL_TEST_EXPECT_EQ_INT(kl_int64_hashmap_unset(&map, KL_BLOB_AUTO(k)), 1);

  int64_t out = 0;
  KL_TEST_EXPECT_EQ_INT(kl_int64_hashmap_get(&map, KL_BLOB_AUTO(k), &out), 0);
  KL_TEST_ASSERT(!kl_int64_hashmap_has(&map, KL_BLOB_AUTO(k)), "has reports false after unset");

  kl_int64_hashmap_destroy(&map);
}

static void test_int64_hashmap_unset_missing_returns_zero() {
  KL_int64_Hashmap map = {0};
  kl_int64_hashmap_init(&map, kl_test_int64_hashmap_default_cfg());
  int64_t k = 1234;
  // Unsetting an absent key is a no-op that returns 0.
  KL_TEST_EXPECT_EQ_INT(kl_int64_hashmap_unset(&map, KL_BLOB_AUTO(k)), 0);
  kl_int64_hashmap_destroy(&map);
}

static void test_int64_hashmap_unset_then_reset_reinserts() {
  // Unset must leave the slot reusable for a future set with any key
  // that hashes to the same home bucket.
  KL_int64_Hashmap map = {0};
  kl_int64_hashmap_init(&map, kl_test_int64_hashmap_default_cfg());

  int64_t k = 9;
  kl_int64_hashmap_set(&map, KL_BLOB_AUTO(k), 100);
  kl_int64_hashmap_unset(&map, KL_BLOB_AUTO(k));
  kl_int64_hashmap_set(&map, KL_BLOB_AUTO(k), 200);

  int64_t out = 0;
  KL_TEST_EXPECT_EQ_INT(kl_int64_hashmap_get(&map, KL_BLOB_AUTO(k), &out), 1);
  KL_TEST_EXPECT_EQ_INT((int)out, 200);

  kl_int64_hashmap_destroy(&map);
}

static void test_int64_hashmap_get_ptr_allows_mutation() {
  KL_int64_Hashmap map = {0};
  kl_int64_hashmap_init(&map, kl_test_int64_hashmap_default_cfg());

  int64_t k = 21;
  kl_int64_hashmap_set(&map, KL_BLOB_AUTO(k), 10);
  int64_t* p = 0;
  KL_TEST_EXPECT_EQ_INT(kl_int64_hashmap_get_ptr(&map, KL_BLOB_AUTO(k), &p), 1);
  KL_TEST_ASSERT(p != 0, "get_ptr non-null");
  KL_TEST_EXPECT_EQ_INT((int)*p, 10);
  *p = 555;

  int64_t out = 0;
  KL_TEST_EXPECT_EQ_INT(kl_int64_hashmap_get(&map, KL_BLOB_AUTO(k), &out), 1);
  KL_TEST_EXPECT_EQ_INT((int)out, 555);

  kl_int64_hashmap_destroy(&map);
}

static void test_int64_hashmap_get_ptr_missing_returns_zero() {
  KL_int64_Hashmap map = {0};
  kl_int64_hashmap_init(&map, kl_test_int64_hashmap_default_cfg());
  int64_t k = 0xBEEF;
  int64_t* p = (int64_t*)0xDEAD;
  KL_TEST_EXPECT_EQ_INT(kl_int64_hashmap_get_ptr(&map, KL_BLOB_AUTO(k), &p), 0);
  kl_int64_hashmap_destroy(&map);
}

static void test_int64_hashmap_hash_is_deterministic() {
  KL_int64_Hashmap map = {0};
  kl_int64_hashmap_init(&map, kl_test_int64_hashmap_default_cfg());

  int64_t k = 123;
  KL_HashmapHash h1 = kl_int64_hashmap_get_hash(&map, KL_BLOB_AUTO(k));
  KL_HashmapHash h2 = kl_int64_hashmap_get_hash(&map, KL_BLOB_AUTO(k));
  KL_TEST_ASSERT(h1.hash == h2.hash, "same key hashes to same value");
  KL_TEST_EXPECT_EQ_INT((int)h1.index, (int)h2.index);
  // Index must be within table bounds.
  KL_TEST_ASSERT(h1.index >= 0 && h1.index < map.capacity_pow2, "index in [0, pow2)");

  kl_int64_hashmap_destroy(&map);
}

static void test_int64_hashmap_collisions_with_tiny_capacity() {
  // capacity=1 → pow2=1 → every key collides on slot 0. This forces the
  // collision-chain + realloc-grow paths to be exercised heavily.
  KL_int64_Hashmap map = {0};
  KL_HashmapConfig cfg = kl_test_int64_hashmap_default_cfg();
  cfg.capacity = 1;
  cfg.collision_capacity = 1;
  kl_int64_hashmap_init(&map, cfg);
  KL_TEST_EXPECT_EQ_INT((int)map.capacity_pow2, 1);

  const int N = 64;
  for (int i = 0; i < N; i++) {
    int64_t k = i + 1; // avoid key value 0
    KL_TEST_ASSERT(kl_int64_hashmap_set(&map, KL_BLOB_AUTO(k), k * 10) >= 0, "set in collide-only map");
  }
  // With a single home bucket, every non-first insert must be a collision.
  KL_TEST_EXPECT_EQ_INT((int)map.n_collisions, N - 1);

  // All keys still resolve to their original values.
  for (int i = 0; i < N; i++) {
    int64_t k = i + 1;
    int64_t out = 0;
    KL_TEST_EXPECT_EQ_INT(kl_int64_hashmap_get(&map, KL_BLOB_AUTO(k), &out), 1);
    KL_TEST_EXPECT_EQ_INT((int)out, (int)(k * 10));
  }

  kl_int64_hashmap_destroy(&map);
}

static void test_int64_hashmap_many_entries_round_trip() {
  KL_int64_Hashmap map = {0};
  KL_HashmapConfig cfg = kl_test_int64_hashmap_default_cfg();
  cfg.capacity = 64;
  cfg.collision_capacity = 64;
  kl_int64_hashmap_init(&map, cfg);

  const int N = 2000;
  for (int i = 0; i < N; i++) {
    int64_t k = i + 1;
    kl_int64_hashmap_set(&map, KL_BLOB_AUTO(k), (int64_t)(i * 3));
  }
  for (int i = 0; i < N; i++) {
    int64_t k = i + 1;
    int64_t out = 0;
    KL_TEST_EXPECT_EQ_INT(kl_int64_hashmap_get(&map, KL_BLOB_AUTO(k), &out), 1);
    KL_TEST_EXPECT_EQ_INT((int)out, i * 3);
  }
  // Sanity: a key never inserted is absent.
  int64_t missing = N + 1234;
  KL_TEST_ASSERT(!kl_int64_hashmap_has(&map, KL_BLOB_AUTO(missing)), "unset key missing");

  kl_int64_hashmap_destroy(&map);
}

static void test_int64_hashmap_unset_in_collision_chain() {
  // Forces multiple collisions, unsets a mid-chain key, then verifies
  // every other key still resolves and only the unset one is gone.
  KL_int64_Hashmap map = {0};
  KL_HashmapConfig cfg = kl_test_int64_hashmap_default_cfg();
  cfg.capacity = 2;
  cfg.collision_capacity = 4;
  kl_int64_hashmap_init(&map, cfg);

  const int N = 16;
  for (int i = 0; i < N; i++) {
    int64_t k = i + 1;
    kl_int64_hashmap_set(&map, KL_BLOB_AUTO(k), (int64_t)(100 + i));
  }
  KL_TEST_ASSERT(map.n_collisions > 0, "tiny table forced collisions");

  // Unset something in the middle of the insertion order.
  int64_t victim = 7;
  KL_TEST_EXPECT_EQ_INT(kl_int64_hashmap_unset(&map, KL_BLOB_AUTO(victim)), 1);

  for (int i = 0; i < N; i++) {
    int64_t k = i + 1;
    int64_t out = 0;
    int hit = kl_int64_hashmap_get(&map, KL_BLOB_AUTO(k), &out);
    if (k == victim) {
      KL_TEST_EXPECT_EQ_INT(hit, 0);
    } else {
      KL_TEST_EXPECT_EQ_INT(hit, 1);
      KL_TEST_EXPECT_EQ_INT((int)out, (int)(100 + i));
    }
  }

  kl_int64_hashmap_destroy(&map);
}

static void test_int64_hashmap_string_keys_fnv_path() {
  // max_key_size > sizeof(int64_t) forces the FNV-1a hashing path.
  KL_int64_Hashmap map = {0};
  KL_HashmapConfig cfg = (KL_HashmapConfig){
    .capacity = 16,
    .collision_capacity = 16,
    .max_key_size = 32,
    .track_used_slots = false,
  };
  kl_int64_hashmap_init(&map, cfg);

  const char* keys[] = { "alpha", "beta", "gamma", "delta", "epsilon", "zeta" };
  int64_t values[] = { 1, 2, 3, 4, 5, 6 };
  size_t n = KL_ARRAY_LEN(keys);

  for (size_t i = 0; i < n; i++) {
    KL_Blob blob = { .value = (uint8_t*)keys[i], .size = (int64_t)strlen(keys[i]) };
    KL_TEST_ASSERT(kl_int64_hashmap_set(&map, blob, values[i]) >= 0, "set string key");
  }
  for (size_t i = 0; i < n; i++) {
    KL_Blob blob = { .value = (uint8_t*)keys[i], .size = (int64_t)strlen(keys[i]) };
    int64_t out = 0;
    KL_TEST_EXPECT_EQ_INT(kl_int64_hashmap_get(&map, blob, &out), 1);
    KL_TEST_EXPECT_EQ_INT((int)out, (int)values[i]);
  }

  // A string that was never inserted is absent.
  KL_Blob miss = { .value = (uint8_t*)"omega", .size = 5 };
  KL_TEST_ASSERT(!kl_int64_hashmap_has(&map, miss), "uninserted string key missing");

  kl_int64_hashmap_destroy(&map);
}

static void test_int64_hashmap_keys_with_same_prefix_differ() {
  // Distinguishes by both bytes and length, even when one is a prefix of another.
  KL_int64_Hashmap map = {0};
  KL_HashmapConfig cfg = (KL_HashmapConfig){
    .capacity = 8,
    .collision_capacity = 8,
    .max_key_size = 32,
    .track_used_slots = false,
  };
  kl_int64_hashmap_init(&map, cfg);

  KL_Blob k_short = { .value = (uint8_t*)"foo",    .size = 3 };
  KL_Blob k_long  = { .value = (uint8_t*)"foobar", .size = 6 };
  kl_int64_hashmap_set(&map, k_short, 10);
  kl_int64_hashmap_set(&map, k_long,  20);

  int64_t out = 0;
  KL_TEST_EXPECT_EQ_INT(kl_int64_hashmap_get(&map, k_short, &out), 1);
  KL_TEST_EXPECT_EQ_INT((int)out, 10);
  KL_TEST_EXPECT_EQ_INT(kl_int64_hashmap_get(&map, k_long, &out), 1);
  KL_TEST_EXPECT_EQ_INT((int)out, 20);

  kl_int64_hashmap_destroy(&map);
}

static void test_int64_hashmap_get_keys_lists_all_set_keys() {
  KL_int64_Hashmap map = {0};
  kl_int64_hashmap_init(&map, kl_test_int64_hashmap_default_cfg());

  int64_t kvs[] = { 11, 22, 33, 44, 55 };
  for (size_t i = 0; i < KL_ARRAY_LEN(kvs); i++) {
    kl_int64_hashmap_set(&map, KL_BLOB_AUTO(kvs[i]), (int64_t)(i + 1));
  }

  KL_blob_ptr_List keys = {0};
  KL_TEST_EXPECT_EQ_INT(kl_int64_hashmap_get_keys(&map, &keys), 1);
  KL_TEST_EXPECT_EQ_INT((int)keys.length, (int)KL_ARRAY_LEN(kvs));

  // Every reported key blob must decode back to one of the inserted ints.
  bool seen[KL_ARRAY_LEN(kvs)] = {0};
  for (int64_t i = 0; i < keys.length; i++) {
    KL_Blob* b = keys.items[i];
    KL_TEST_ASSERT(b != 0 && b->value != 0, "key blob non-null");
    KL_TEST_EXPECT_EQ_INT((int)b->size, (int)sizeof(int64_t));
    int64_t decoded = 0;
    memcpy(&decoded, b->value, sizeof(int64_t));
    bool found = false;
    for (size_t j = 0; j < KL_ARRAY_LEN(kvs); j++) {
      if (kvs[j] == decoded && !seen[j]) { seen[j] = true; found = true; break; }
    }
    KL_TEST_ASSERT(found, "reported key matches an inserted key exactly once");
  }

  kl_blob_ptr_list_destroy(&keys);
  kl_int64_hashmap_destroy(&map);
}

static void test_int64_hashmap_get_keys_excludes_unset() {
  KL_int64_Hashmap map = {0};
  kl_int64_hashmap_init(&map, kl_test_int64_hashmap_default_cfg());

  int64_t a = 1, b = 2, c = 3;
  kl_int64_hashmap_set(&map, KL_BLOB_AUTO(a), 10);
  kl_int64_hashmap_set(&map, KL_BLOB_AUTO(b), 20);
  kl_int64_hashmap_set(&map, KL_BLOB_AUTO(c), 30);
  kl_int64_hashmap_unset(&map, KL_BLOB_AUTO(b));

  KL_blob_ptr_List keys = {0};
  kl_int64_hashmap_get_keys(&map, &keys);
  KL_TEST_EXPECT_EQ_INT((int)keys.length, 2);
  for (int64_t i = 0; i < keys.length; i++) {
    int64_t decoded = 0;
    memcpy(&decoded, keys.items[i]->value, sizeof(int64_t));
    KL_TEST_ASSERT(decoded != b, "unset key not reported");
  }
  kl_blob_ptr_list_destroy(&keys);
  kl_int64_hashmap_destroy(&map);
}

static void test_int64_hashmap_used_slots_tracking() {
  // track_used_slots populates a parallel list of slot indices for set entries.
  KL_int64_Hashmap map = {0};
  KL_HashmapConfig cfg = kl_test_int64_hashmap_default_cfg();
  cfg.track_used_slots = true;
  kl_int64_hashmap_init(&map, cfg);
  KL_TEST_ASSERT(map.used_slots.initialized, "used_slots list initialized when configured");

  int64_t k1 = 1, k2 = 2, k3 = 3;
  int64_t i1 = kl_int64_hashmap_set(&map, KL_BLOB_AUTO(k1), 10);
  int64_t i2 = kl_int64_hashmap_set(&map, KL_BLOB_AUTO(k2), 20);
  int64_t i3 = kl_int64_hashmap_set(&map, KL_BLOB_AUTO(k3), 30);
  KL_TEST_EXPECT_EQ_INT((int)map.used_slots.length, 3);

  // Overwriting an existing key must NOT add another entry.
  kl_int64_hashmap_set(&map, KL_BLOB_AUTO(k2), 222);
  KL_TEST_EXPECT_EQ_INT((int)map.used_slots.length, 3);

  // After unset, the slot index drops out of the list.
  kl_int64_hashmap_unset(&map, KL_BLOB_AUTO(k2));
  KL_TEST_EXPECT_EQ_INT((int)map.used_slots.length, 2);
  for (int64_t i = 0; i < map.used_slots.length; i++) {
    KL_TEST_ASSERT(map.used_slots.items[i] != i2, "unset slot index removed from used_slots");
    KL_TEST_ASSERT(map.used_slots.items[i] == i1 || map.used_slots.items[i] == i3,
                   "remaining indices match inserted slots");
  }

  kl_int64_hashmap_destroy(&map);
}

static void test_int64_hashmap_get_used_slots_returns_set_only() {
  KL_int64_Hashmap map = {0};
  kl_int64_hashmap_init(&map, kl_test_int64_hashmap_default_cfg());

  int64_t kvs[] = { 100, 200, 300, 400 };
  for (size_t i = 0; i < KL_ARRAY_LEN(kvs); i++) {
    kl_int64_hashmap_set(&map, KL_BLOB_AUTO(kvs[i]), (int64_t)(i + 1));
  }
  kl_int64_hashmap_unset(&map, KL_BLOB_AUTO(kvs[1]));

  KL_int64_hashmap_slot_List out = {0};
  kl_int64_hashmap_get_used_slots(&map, &out);
  KL_TEST_EXPECT_EQ_INT((int)out.length, (int)(KL_ARRAY_LEN(kvs) - 1));
  for (int64_t i = 0; i < out.length; i++) {
    KL_TEST_ASSERT(out.items[i].is_set, "reported slot is set");
    KL_TEST_ASSERT(out.items[i].key.value != 0, "reported slot has a key");
  }
  kl_int64_hashmap_slot_list_destroy(&out);
  kl_int64_hashmap_destroy(&map);
}

static void test_int64_hashmap_reset_clears_entries() {
  KL_int64_Hashmap map = {0};
  KL_HashmapConfig cfg = kl_test_int64_hashmap_default_cfg();
  kl_int64_hashmap_init(&map, cfg);

  for (int i = 0; i < 32; i++) {
    int64_t k = i + 1;
    kl_int64_hashmap_set(&map, KL_BLOB_AUTO(k), (int64_t)(i + 1));
  }
  kl_int64_hashmap_reset(&map);

  // Config is preserved and table is empty.
  KL_TEST_ASSERT(map.initialized, "still initialized after reset");
  KL_TEST_EXPECT_EQ_INT((int)map.config.capacity, (int)cfg.capacity);
  KL_TEST_EXPECT_EQ_INT((int)map.n_collisions, 0);
  for (int i = 0; i < 32; i++) {
    int64_t k = i + 1;
    KL_TEST_ASSERT(!kl_int64_hashmap_has(&map, KL_BLOB_AUTO(k)), "no entry survives reset");
  }
  // Reusable afterwards.
  int64_t k = 999;
  kl_int64_hashmap_set(&map, KL_BLOB_AUTO(k), 42);
  int64_t out = 0;
  KL_TEST_EXPECT_EQ_INT(kl_int64_hashmap_get(&map, KL_BLOB_AUTO(k), &out), 1);
  KL_TEST_EXPECT_EQ_INT((int)out, 42);

  kl_int64_hashmap_destroy(&map);
}

static void test_int64_hashmap_serialize_deserialize_round_trip() {
  KL_int64_Hashmap src = {0};
  KL_HashmapConfig cfg = kl_test_int64_hashmap_default_cfg();
  cfg.capacity = 8;
  cfg.collision_capacity = 8;
  kl_int64_hashmap_init(&src, cfg);

  const int N = 50;
  for (int i = 0; i < N; i++) {
    int64_t k = i + 1;
    kl_int64_hashmap_set(&src, KL_BLOB_AUTO(k), (int64_t)(i * 7 + 1));
  }
  // Drop one entry to exercise is_set=false serialization too.
  int64_t dropped = 5;
  kl_int64_hashmap_unset(&src, KL_BLOB_AUTO(dropped));

  FILE* fp = tmpfile();
  KL_TEST_ASSERT(fp != 0, "tmpfile non-null");
  kl_int64_hashmap_serialize_to_file(src, fp);
  rewind(fp);

  KL_int64_Hashmap dst = {0};
  KL_TEST_EXPECT_EQ_INT(kl_int64_hashmap_deserialize_from_file(&dst, fp), 1);
  fclose(fp);

  KL_TEST_ASSERT(dst.initialized, "dst initialized after deserialize");
  KL_TEST_EXPECT_EQ_INT((int)dst.config.capacity, (int)cfg.capacity);

  // Every still-set key must round-trip; the unset key must remain absent.
  for (int i = 0; i < N; i++) {
    int64_t k = i + 1;
    int64_t out = 0;
    if (k == dropped) {
      KL_TEST_EXPECT_EQ_INT(kl_int64_hashmap_get(&dst, KL_BLOB_AUTO(k), &out), 0);
    } else {
      KL_TEST_EXPECT_EQ_INT(kl_int64_hashmap_get(&dst, KL_BLOB_AUTO(k), &out), 1);
      KL_TEST_EXPECT_EQ_INT((int)out, i * 7 + 1);
    }
  }

  kl_int64_hashmap_destroy(&src);
  kl_int64_hashmap_destroy(&dst);
}

static void test_int64_hashmap_deserialize_rejects_bad_magic() {
  // Writing arbitrary bytes where the magic header is expected must be rejected.
  FILE* fp = tmpfile();
  KL_TEST_ASSERT(fp != 0, "tmpfile non-null");
  uint32_t bogus = 0xCAFEBABE;
  fwrite(&bogus, sizeof(bogus), 1, fp);
  rewind(fp);

  KL_int64_Hashmap dst = {0};
  // The deserializer logs to stderr on bad magic; muffle so test output stays tidy.
  //int saved_err = dup(2);
  //FILE* devnull = fopen("/dev/null", "w");
  //if (devnull != 0) dup2(fileno(devnull), 2);
  int rc = kl_int64_hashmap_deserialize_from_file(&dst, fp);
  //fflush(stderr);
  //if (devnull != 0) { dup2(saved_err, 2); fclose(devnull); }
  //close(saved_err);

  KL_TEST_EXPECT_EQ_INT(rc, 0);
  KL_TEST_ASSERT(!dst.initialized, "dst left uninitialized on bad magic");
  fclose(fp);
}

static void test_int64_hashmap_zero_value_is_distinguishable() {
  // Value type is int64_t and 0 is a valid value; get must report
  // not-found and present cases distinctly even when the value is 0.
  KL_int64_Hashmap map = {0};
  kl_int64_hashmap_init(&map, kl_test_int64_hashmap_default_cfg());

  int64_t k = 314;
  int64_t out = 0xAB;
  KL_TEST_EXPECT_EQ_INT(kl_int64_hashmap_get(&map, KL_BLOB_AUTO(k), &out), 0);
  KL_TEST_EXPECT_EQ_INT((int)out, 0xAB);

  kl_int64_hashmap_set(&map, KL_BLOB_AUTO(k), 0);
  KL_TEST_EXPECT_EQ_INT(kl_int64_hashmap_get(&map, KL_BLOB_AUTO(k), &out), 1);
  KL_TEST_EXPECT_EQ_INT((int)out, 0);

  kl_int64_hashmap_destroy(&map);
}

static void test_int64_hashmap_negative_and_extreme_keys() {
  KL_int64_Hashmap map = {0};
  kl_int64_hashmap_init(&map, kl_test_int64_hashmap_default_cfg());

  int64_t keys[] = { INT64_MIN, -1, 1, INT64_MAX };
  int64_t vals[] = { -100, -1, 2, 999999 };
  for (size_t i = 0; i < KL_ARRAY_LEN(keys); i++) {
    kl_int64_hashmap_set(&map, KL_BLOB_AUTO(keys[i]), vals[i]);
  }
  for (size_t i = 0; i < KL_ARRAY_LEN(keys); i++) {
    int64_t out = 0;
    KL_TEST_EXPECT_EQ_INT(kl_int64_hashmap_get(&map, KL_BLOB_AUTO(keys[i]), &out), 1);
    KL_TEST_ASSERT(out == vals[i], "extreme key round-trips");
  }

  kl_int64_hashmap_destroy(&map);
}


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

static void test_binary_sparseset_init_destroy() {
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

static void test_binary_sparseset_double_init_is_noop() {
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

static void test_binary_sparseset_set_get_basic() {
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

static void test_binary_sparseset_get_missing_returns_null() {
  KL_TestBSS fx = {0};
  kl_test_bss_setup(&fx, sizeof(int64_t), _Alignof(int64_t), 16, 8);

  KL_TEST_ASSERT(kl_binary_sparseset_get(&fx.set, 0) == 0, "get on empty returns null");
  KL_TEST_ASSERT(kl_binary_sparseset_get(&fx.set, 5) == 0, "get unset id returns null");

  int64_t v = 1;
  kl_binary_sparseset_set(&fx.set, 7, &v);
  KL_TEST_ASSERT(kl_binary_sparseset_get(&fx.set, 6) == 0, "neighbouring id still null");

  kl_test_bss_teardown(&fx);
}

static void test_binary_sparseset_set_null_data_zero_inits() {
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

static void test_binary_sparseset_set_overwrite_keeps_length() {
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

static void test_binary_sparseset_set_returns_pointer_into_values() {
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

static void test_binary_sparseset_has_before_and_after_set() {
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

static void test_binary_sparseset_has_out_of_range_returns_false() {
  KL_TestBSS fx = {0};
  kl_test_bss_setup(&fx, sizeof(int64_t), _Alignof(int64_t), 16, 8);

  KL_TEST_ASSERT(!kl_binary_sparseset_has(&fx.set, -1), "negative id => false");
  KL_TEST_ASSERT(!kl_binary_sparseset_has(&fx.set, fx.set.sparse_capacity), "id == capacity => false");
  KL_TEST_ASSERT(!kl_binary_sparseset_has(&fx.set, 100000), "huge id => false");

  kl_test_bss_teardown(&fx);
}

static void test_binary_sparseset_get_oob_returns_null() {
  KL_TestBSS fx = {0};
  kl_test_bss_setup(&fx, sizeof(int64_t), _Alignof(int64_t), 16, 8);

  KL_TEST_ASSERT(kl_binary_sparseset_get(&fx.set, -1) == 0, "negative id => null");
  KL_TEST_ASSERT(kl_binary_sparseset_get(&fx.set, fx.set.sparse_capacity) == 0, "id == capacity => null");

  kl_test_bss_teardown(&fx);
}

static void test_binary_sparseset_remove_missing_returns_false() {
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

static void test_binary_sparseset_remove_last_element_no_swap() {
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

static void test_binary_sparseset_remove_middle_swaps_with_last() {
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

static void test_binary_sparseset_remove_then_reinsert_same_id() {
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

static void test_binary_sparseset_remove_all_drains() {
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

static void test_binary_sparseset_reset_clears_entries() {
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

static void test_binary_sparseset_reset_preserves_capacity() {
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

static void test_binary_sparseset_reset_on_empty_is_safe() {
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

static void test_binary_sparseset_sparse_grows_for_large_id() {
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

static void test_binary_sparseset_dense_grows_when_full() {
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

static void test_binary_sparseset_at_iterates_dense_buffer() {
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

static void test_binary_sparseset_struct_payload_round_trip() {
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

static void test_binary_sparseset_many_inserts_round_trip() {
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

static void test_binary_sparseset_alignment_default() {
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

static void test_binary_sparseset_id_zero_works() {
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

static void test_str_const_slice() {
  KL_TEST_EXPECT_EXPR(kl_str_const_eq(kl_str_const_slice(KL_STR_CONST_LIT("hello world"), 0, 5), KL_STR_CONST_LIT("hello")));
  KL_TEST_EXPECT_EXPR(kl_str_const_eq(kl_str_const_slice(KL_STR_CONST_LIT("hello world"), 6, 5), KL_STR_CONST_LIT("world")));
}

static void test_str_const_includes() {
  // positive cases
  KL_TEST_EXPECT_EXPR(kl_str_const_includes(KL_STR_CONST_LIT("the quick brown fox jumps over the lazy dog"), KL_STR_CONST_LIT("the")));
  KL_TEST_EXPECT_EXPR(kl_str_const_includes(KL_STR_CONST_LIT("the quick brown fox jumps over the lazy dog"), KL_STR_CONST_LIT("quick")));
  KL_TEST_EXPECT_EXPR(kl_str_const_includes(KL_STR_CONST_LIT("the quick brown fox jumps over the lazy dog"), KL_STR_CONST_LIT("brown")));
  KL_TEST_EXPECT_EXPR(kl_str_const_includes(KL_STR_CONST_LIT("the quick brown fox jumps over the lazy dog"), KL_STR_CONST_LIT("fox")));
  KL_TEST_EXPECT_EXPR(kl_str_const_includes(KL_STR_CONST_LIT("the quick brown fox jumps over the lazy dog"), KL_STR_CONST_LIT("jumps")));
  KL_TEST_EXPECT_EXPR(kl_str_const_includes(KL_STR_CONST_LIT("the quick brown fox jumps over the lazy dog"), KL_STR_CONST_LIT("over")));
  KL_TEST_EXPECT_EXPR(kl_str_const_includes(KL_STR_CONST_LIT("the quick brown fox jumps over the lazy dog"), KL_STR_CONST_LIT("the")));
  KL_TEST_EXPECT_EXPR(kl_str_const_includes(KL_STR_CONST_LIT("the quick brown fox jumps over the lazy dog"), KL_STR_CONST_LIT("lazy")));
  KL_TEST_EXPECT_EXPR(kl_str_const_includes(KL_STR_CONST_LIT("the quick brown fox jumps over the lazy dog"), KL_STR_CONST_LIT("dog")));

  // negative cases
  KL_TEST_EXPECT_EXPR(!kl_str_const_includes(KL_STR_CONST_LIT("the quick brown fox jumps over the lazy dog"), KL_STR_CONST_LIT("foo")));
  KL_TEST_EXPECT_EXPR(!kl_str_const_includes(KL_STR_CONST_LIT("the quick brown fox jumps over the lazy dog"), KL_STR_CONST_LIT("bar")));
  KL_TEST_EXPECT_EXPR(!kl_str_const_includes(KL_STR_CONST_LIT("the quick brown fox jumps over the lazy dog"), KL_STR_CONST_LIT("baz")));
  KL_TEST_EXPECT_EXPR(!kl_str_const_includes(KL_STR_CONST_LIT("the quick brown fox jumps over the lazy dog"), KL_STR_CONST_LIT("foobar")));
  KL_TEST_EXPECT_EXPR(!kl_str_const_includes(KL_STR_CONST_LIT("the quick brown fox jumps over the lazy dog"), KL_STR_CONST_LIT("bazbar")));
  KL_TEST_EXPECT_EXPR(!kl_str_const_includes(KL_STR_CONST_LIT("the quick brown fox jumps over the lazy dog"), KL_STR_CONST_LIT("foobaz")));
  KL_TEST_EXPECT_EXPR(!kl_str_const_includes(KL_STR_CONST_LIT("the quick brown fox jumps over the lazy dog"), KL_STR_CONST_LIT("bazfoo")));
  KL_TEST_EXPECT_EXPR(!kl_str_const_includes(KL_STR_CONST_LIT("the quick brown fox jumps over the lazy dog"), KL_STR_CONST_LIT("john doe")));
  KL_TEST_EXPECT_EXPR(!kl_str_const_includes(KL_STR_CONST_LIT("the quick brown fox jumps over the lazy dog"), KL_STR_CONST_LIT("abc123")));
}


static void test_fs_read_directory_rejects_bad_args() {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 4096 });
  KL_file_List files = {0};

  // null value
  KL_TEST_EXPECT_EXPR(!kl_fs_read_directory((KL_StrConst){0}, false, &a, &files));
  // zero length
  KL_TEST_EXPECT_EXPR(!kl_fs_read_directory((KL_StrConst){ .value = "src", .length = 0 }, false, &a, &files));
  // null out_files
  KL_TEST_EXPECT_EXPR(!kl_fs_read_directory(KL_STR_CONST_LIT("src"), false, &a, 0));
  // null allocator
  KL_TEST_EXPECT_EXPR(!kl_fs_read_directory(KL_STR_CONST_LIT("src"), false, 0, &files));

  kl_allocator_destroy(&a);
}

static void test_fs_read_directory_lists_project_src_non_recursive() {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 4096 });

  KL_file_List files = {0};
  bool ok = kl_fs_read_directory(KL_STR_CONST_LIT("src"), false, &a, &files);
  KL_TEST_ASSERT(ok, "kl_fs_read_directory returned true for 'src'");
  KL_TEST_ASSERT(files.initialized, "out_files auto-initialized");
  KL_TEST_ASSERT(files.length > 0, "got at least one entry");
  KL_TEST_ASSERT(files.items != 0, "items buffer is non-null");

  // Every entry should have a non-null name and parent_path matching the input.
  bool found_io = false;
  bool found_memory = false;
  bool found_string = false;
  for (int64_t i = 0; i < files.length; i++) {
    KL_File f = files.items[i];
    KL_TEST_ASSERT(f.name.value != 0, "entry name.value is non-null");
    KL_TEST_ASSERT(f.name.length > 0 && f.name.length < 4096, "entry name.length is sane");
    KL_TEST_EXPECT_EQ_INT((int)f.name.length, (int)strlen(f.name.value));
    KL_TEST_EXPECT_EXPR(kl_str_const_eq(f.parent_path, KL_STR_CONST_LIT("src")));
    // The dummy "." and ".." entries must have been skipped.
    KL_TEST_EXPECT_EXPR(!kl_str_const_eq(f.name, KL_STR_CONST_LIT(".")));
    KL_TEST_EXPECT_EXPR(!kl_str_const_eq(f.name, KL_STR_CONST_LIT("..")));

    if (kl_str_const_eq(f.name, KL_STR_CONST_LIT("io"))) {
      KL_TEST_ASSERT(f.is_directory, "src/io is a directory");
      found_io = true;
    }
    if (kl_str_const_eq(f.name, KL_STR_CONST_LIT("memory"))) {
      KL_TEST_ASSERT(f.is_directory, "src/memory is a directory");
      found_memory = true;
    }
    if (kl_str_const_eq(f.name, KL_STR_CONST_LIT("string"))) {
      KL_TEST_ASSERT(f.is_directory, "src/string is a directory");
      found_string = true;
    }
  }
  KL_TEST_ASSERT(found_io, "non-recursive listing of 'src' contains 'io'");
  KL_TEST_ASSERT(found_memory, "non-recursive listing of 'src' contains 'memory'");
  KL_TEST_ASSERT(found_string, "non-recursive listing of 'src' contains 'string'");

  kl_allocator_destroy(&a);
}

static void test_fs_read_directory_recursive_descends_into_subdirs() {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 16384 });

  KL_file_List files = {0};
  bool ok = kl_fs_read_directory(KL_STR_CONST_LIT("src"), true, &a, &files);
  KL_TEST_ASSERT(ok, "recursive kl_fs_read_directory returned true for 'src'");

  // A recursive walk must surface strictly more entries than a non-recursive one,
  // because 'src' has nested subdirectories with files.
  KL_file_List shallow = {0};
  KL_TEST_EXPECT_EXPR(kl_fs_read_directory(KL_STR_CONST_LIT("src"), false, &a, &shallow));
  KL_TEST_ASSERT(files.length > shallow.length, "recursive yields more entries than non-recursive");

  // The recursive walk should reach a known leaf file: src/io/fs.c.
  bool found_fs_c = false;
  for (int64_t i = 0; i < files.length; i++) {
    KL_File f = files.items[i];
    if (!f.is_directory
        && kl_str_const_eq(f.name, KL_STR_CONST_LIT("fs.c"))
        && kl_str_const_ends_with(f.parent_path, KL_STR_CONST_LIT("io"))) {
      found_fs_c = true;
      break;
    }
  }
  KL_TEST_ASSERT(found_fs_c, "recursive walk reached src/io/fs.c");

  kl_allocator_destroy(&a);
}

static void run_test(KL_TestEntry* entry) {
  KL_TEST_BEGIN(entry->name);
  entry->fn();
  KL_TEST_END(entry->name);
}

int main(int argc, char* argv[]) {
  KL_TestEntry tests[] = {
    KL_TEST_ENTRY(test_str_const_slice),
    KL_TEST_ENTRY(test_str_const_includes),
    
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

    KL_TEST_ENTRY(test_int64_hashmap_init_destroy),
    KL_TEST_ENTRY(test_int64_hashmap_double_init_is_noop),
    KL_TEST_ENTRY(test_int64_hashmap_capacity_rounds_to_power_of_two),
    KL_TEST_ENTRY(test_int64_hashmap_set_get_basic),
    KL_TEST_ENTRY(test_int64_hashmap_get_missing_returns_zero),
    KL_TEST_ENTRY(test_int64_hashmap_set_overwrite_updates_value),
    KL_TEST_ENTRY(test_int64_hashmap_set_returns_valid_slot_index),
    KL_TEST_ENTRY(test_int64_hashmap_has_before_and_after),
    KL_TEST_ENTRY(test_int64_hashmap_unset_removes_entry),
    KL_TEST_ENTRY(test_int64_hashmap_unset_missing_returns_zero),
    KL_TEST_ENTRY(test_int64_hashmap_unset_then_reset_reinserts),
    KL_TEST_ENTRY(test_int64_hashmap_get_ptr_allows_mutation),
    KL_TEST_ENTRY(test_int64_hashmap_get_ptr_missing_returns_zero),
    KL_TEST_ENTRY(test_int64_hashmap_hash_is_deterministic),
    KL_TEST_ENTRY(test_int64_hashmap_collisions_with_tiny_capacity),
    KL_TEST_ENTRY(test_int64_hashmap_many_entries_round_trip),
    KL_TEST_ENTRY(test_int64_hashmap_unset_in_collision_chain),
    KL_TEST_ENTRY(test_int64_hashmap_string_keys_fnv_path),
    KL_TEST_ENTRY(test_int64_hashmap_keys_with_same_prefix_differ),
    KL_TEST_ENTRY(test_int64_hashmap_get_keys_lists_all_set_keys),
    KL_TEST_ENTRY(test_int64_hashmap_get_keys_excludes_unset),
    KL_TEST_ENTRY(test_int64_hashmap_used_slots_tracking),
    KL_TEST_ENTRY(test_int64_hashmap_get_used_slots_returns_set_only),
    KL_TEST_ENTRY(test_int64_hashmap_reset_clears_entries),
    KL_TEST_ENTRY(test_int64_hashmap_serialize_deserialize_round_trip),
    KL_TEST_ENTRY(test_int64_hashmap_deserialize_rejects_bad_magic),
    KL_TEST_ENTRY(test_int64_hashmap_zero_value_is_distinguishable),
    KL_TEST_ENTRY(test_int64_hashmap_negative_and_extreme_keys),

    KL_TEST_ENTRY(test_binary_sparseset_init_destroy),
    KL_TEST_ENTRY(test_binary_sparseset_double_init_is_noop),
    KL_TEST_ENTRY(test_binary_sparseset_set_get_basic),
    KL_TEST_ENTRY(test_binary_sparseset_get_missing_returns_null),
    KL_TEST_ENTRY(test_binary_sparseset_set_null_data_zero_inits),
    KL_TEST_ENTRY(test_binary_sparseset_set_overwrite_keeps_length),
    KL_TEST_ENTRY(test_binary_sparseset_set_returns_pointer_into_values),
    KL_TEST_ENTRY(test_binary_sparseset_has_before_and_after_set),
    KL_TEST_ENTRY(test_binary_sparseset_has_out_of_range_returns_false),
    KL_TEST_ENTRY(test_binary_sparseset_get_oob_returns_null),
    KL_TEST_ENTRY(test_binary_sparseset_remove_missing_returns_false),
    KL_TEST_ENTRY(test_binary_sparseset_remove_last_element_no_swap),
    KL_TEST_ENTRY(test_binary_sparseset_remove_middle_swaps_with_last),
    KL_TEST_ENTRY(test_binary_sparseset_remove_then_reinsert_same_id),
    KL_TEST_ENTRY(test_binary_sparseset_remove_all_drains),
    KL_TEST_ENTRY(test_binary_sparseset_reset_clears_entries),
    KL_TEST_ENTRY(test_binary_sparseset_reset_preserves_capacity),
    KL_TEST_ENTRY(test_binary_sparseset_reset_on_empty_is_safe),
    KL_TEST_ENTRY(test_binary_sparseset_sparse_grows_for_large_id),
    KL_TEST_ENTRY(test_binary_sparseset_dense_grows_when_full),
    KL_TEST_ENTRY(test_binary_sparseset_at_iterates_dense_buffer),
    KL_TEST_ENTRY(test_binary_sparseset_struct_payload_round_trip),
    KL_TEST_ENTRY(test_binary_sparseset_many_inserts_round_trip),
    KL_TEST_ENTRY(test_binary_sparseset_alignment_default),
    KL_TEST_ENTRY(test_binary_sparseset_id_zero_works),

    KL_TEST_ENTRY(test_fs_read_directory_rejects_bad_args),
    KL_TEST_ENTRY(test_fs_read_directory_lists_project_src_non_recursive),
    KL_TEST_ENTRY(test_fs_read_directory_recursive_descends_into_subdirs),
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
