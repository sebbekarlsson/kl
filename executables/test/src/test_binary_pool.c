#include <test/tests.h>
#include <kl/templates/pool/binary_pool.h>
#include <stddef.h>

// ---------- binary_pool ----------

typedef struct {
  KL_Allocator a;
  KL_BinaryPool pool;
} KL_TestBP;

static void kl_test_bp_setup(KL_TestBP* fx, int64_t count, int64_t element_size,
                             int64_t element_alignment) {
  kl_allocator_init(&fx->a, (KL_AllocatorConfig){
    .capacity = 64 * 1024,
    .track_allocations = false,
  });
  fx->pool = (KL_BinaryPool){0};
  kl_binary_pool_init(&fx->pool, (KL_BinaryPoolConfig){
    .count = count,
    .element_size = element_size,
    .element_alignment = element_alignment,
    .allocator = &fx->a,
  });
}

static void kl_test_bp_teardown(KL_TestBP* fx) {
  kl_binary_pool_destroy(&fx->pool);
  kl_allocator_destroy(&fx->a);
}

void test_binary_pool_init_destroy() {
  KL_TestBP fx = {0};
  kl_test_bp_setup(&fx, 8, sizeof(int64_t), _Alignof(int64_t));

  KL_TEST_ASSERT(fx.pool.initialized == true, "initialized after init");
  KL_TEST_EXPECT_EQ_INT((int)fx.pool.config.count, 8);
  KL_TEST_EXPECT_EQ_INT((int)fx.pool.config.element_size, (int)sizeof(int64_t));
  KL_TEST_EXPECT_EQ_INT((int)fx.pool.config.element_alignment, (int)_Alignof(int64_t));
  KL_TEST_ASSERT(fx.pool.items == 0, "items deferred until first acquire");
  KL_TEST_ASSERT(fx.pool.taken_pos == 0, "taken_pos deferred until first acquire");

  kl_binary_pool_destroy(&fx.pool);
  KL_TEST_ASSERT(fx.pool.initialized == false, "initialized cleared after destroy");
  KL_TEST_ASSERT(fx.pool.items == 0, "items cleared after destroy");
  KL_TEST_ASSERT(fx.pool.taken_pos == 0, "taken_pos cleared after destroy");

  kl_allocator_destroy(&fx.a);
}

void test_binary_pool_double_init_is_noop() {
  KL_TestBP fx = {0};
  kl_test_bp_setup(&fx, 4, sizeof(int32_t), _Alignof(int32_t));

  int64_t count_before = fx.pool.config.count;
  int64_t size_before = fx.pool.config.element_size;

  kl_binary_pool_init(&fx.pool, (KL_BinaryPoolConfig){
    .count = 9999,
    .element_size = 4096,
    .element_alignment = 64,
    .allocator = &fx.a,
  });
  KL_TEST_EXPECT_EQ_INT((int)fx.pool.config.count, (int)count_before);
  KL_TEST_EXPECT_EQ_INT((int)fx.pool.config.element_size, (int)size_before);

  kl_test_bp_teardown(&fx);
}

void test_binary_pool_acquire_basic() {
  KL_TestBP fx = {0};
  kl_test_bp_setup(&fx, 8, sizeof(int64_t), _Alignof(int64_t));

  int64_t idx = -1;
  uint8_t* slot = kl_binary_pool_acquire_slot(&fx.pool, &idx);
  KL_TEST_ASSERT(slot != 0, "acquire returns non-null slot");
  KL_TEST_ASSERT(idx >= 0 && idx < 8, "out_idx is in range");
  KL_TEST_ASSERT(fx.pool.items != 0, "items allocated after first acquire");
  KL_TEST_ASSERT(fx.pool.taken_pos != 0, "taken_pos allocated after first acquire");
  KL_TEST_ASSERT(fx.pool.taken_pos[idx] >= 0, "acquired slot recorded in taken_pos");

  kl_test_bp_teardown(&fx);
}

void test_binary_pool_acquire_returns_distinct_slots() {
  KL_TestBP fx = {0};
  kl_test_bp_setup(&fx, 4, sizeof(int64_t), _Alignof(int64_t));

  int64_t i0 = -1, i1 = -1, i2 = -1;
  uint8_t* p0 = kl_binary_pool_acquire_slot(&fx.pool, &i0);
  uint8_t* p1 = kl_binary_pool_acquire_slot(&fx.pool, &i1);
  uint8_t* p2 = kl_binary_pool_acquire_slot(&fx.pool, &i2);

  KL_TEST_ASSERT(p0 != 0 && p1 != 0 && p2 != 0, "all acquires succeed");
  KL_TEST_ASSERT(i0 != i1 && i1 != i2 && i0 != i2, "indices are distinct");
  KL_TEST_ASSERT(p0 != p1 && p1 != p2 && p0 != p2, "pointers are distinct");

  kl_test_bp_teardown(&fx);
}

void test_binary_pool_acquire_respects_element_stride() {
  KL_TestBP fx = {0};
  // 24-byte struct with 8-byte alignment — stride should be 24.
  kl_test_bp_setup(&fx, 4, 24, 8);

  int64_t i0 = -1, i1 = -1;
  uint8_t* p0 = kl_binary_pool_acquire_slot(&fx.pool, &i0);
  uint8_t* p1 = kl_binary_pool_acquire_slot(&fx.pool, &i1);
  KL_TEST_ASSERT(p0 != 0 && p1 != 0, "two acquires succeed");

  // Two consecutive acquires must return adjacent slots regardless of which
  // order free_indices hands them out. Their pointer delta must equal the
  // per-element stride (here 24).
  ptrdiff_t delta = (i1 > i0) ? (p1 - p0) : (p0 - p1);
  KL_TEST_EXPECT_EQ_INT((int)delta, 24);

  kl_test_bp_teardown(&fx);
}

void test_binary_pool_acquire_pads_size_to_alignment() {
  KL_TestBP fx = {0};
  // element_size=5, alignment=8 — stride must round up to 8.
  kl_test_bp_setup(&fx, 4, 5, 8);

  int64_t i0 = -1, i1 = -1;
  uint8_t* p0 = kl_binary_pool_acquire_slot(&fx.pool, &i0);
  uint8_t* p1 = kl_binary_pool_acquire_slot(&fx.pool, &i1);
  KL_TEST_ASSERT(p0 != 0 && p1 != 0, "two acquires succeed");

  ptrdiff_t delta = (i1 > i0) ? (p1 - p0) : (p0 - p1);
  KL_TEST_EXPECT_EQ_INT((int)delta, 8);
  KL_TEST_ASSERT(((uintptr_t)p0 % 8) == 0, "slot 0 is 8-byte aligned");
  KL_TEST_ASSERT(((uintptr_t)p1 % 8) == 0, "slot 1 is 8-byte aligned");

  kl_test_bp_teardown(&fx);
}

void test_binary_pool_write_round_trip() {
  KL_TestBP fx = {0};
  kl_test_bp_setup(&fx, 4, sizeof(int64_t), _Alignof(int64_t));

  int64_t i0 = -1, i1 = -1;
  int64_t* a = (int64_t*)kl_binary_pool_acquire_slot(&fx.pool, &i0);
  int64_t* b = (int64_t*)kl_binary_pool_acquire_slot(&fx.pool, &i1);
  KL_TEST_ASSERT(a != 0 && b != 0, "acquires succeed");

  *a = 0x1234567890abcdefLL;
  *b = -42;

  // Reads via get_slot for the same indices return the same values, i.e. writes
  // landed in non-overlapping memory of the right width.
  int64_t* ga = (int64_t*)kl_binary_pool_get_slot(&fx.pool, i0);
  int64_t* gb = (int64_t*)kl_binary_pool_get_slot(&fx.pool, i1);
  KL_TEST_ASSERT(*ga == 0x1234567890abcdefLL, "slot a value round-trips");
  KL_TEST_ASSERT(*gb == -42, "slot b value round-trips");

  kl_test_bp_teardown(&fx);
}

typedef struct {
  int64_t id;
  double weight;
  int32_t flags;
} KL_TestPoolItem;

void test_binary_pool_struct_payload_round_trip() {
  KL_TestBP fx = {0};
  kl_test_bp_setup(&fx, 8, sizeof(KL_TestPoolItem), _Alignof(KL_TestPoolItem));

  int64_t indices[8];
  KL_TestPoolItem* slots[8];
  for (int i = 0; i < 8; i++) {
    slots[i] = (KL_TestPoolItem*)kl_binary_pool_acquire_slot(&fx.pool, &indices[i]);
    KL_TEST_ASSERT(slots[i] != 0, "acquire succeeds");
    slots[i]->id = 1000 + i;
    slots[i]->weight = 0.5 * (double)i;
    slots[i]->flags = i * i;
  }

  for (int i = 0; i < 8; i++) {
    KL_TestPoolItem* got = (KL_TestPoolItem*)kl_binary_pool_get_slot(&fx.pool, indices[i]);
    KL_TEST_ASSERT(got != 0, "get returns non-null");
    KL_TEST_EXPECT_EQ_INT((int)got->id, 1000 + i);
    KL_TEST_ASSERT(got->weight == 0.5 * (double)i, "weight round-trips");
    KL_TEST_EXPECT_EQ_INT((int)got->flags, i * i);
  }

  kl_test_bp_teardown(&fx);
}

void test_binary_pool_acquire_exhausted_returns_zero() {
  KL_TestBP fx = {0};
  kl_test_bp_setup(&fx, 3, sizeof(int32_t), _Alignof(int32_t));

  int64_t idx = -1;
  KL_TEST_ASSERT(kl_binary_pool_acquire_slot(&fx.pool, &idx) != 0, "acquire 1");
  KL_TEST_ASSERT(kl_binary_pool_acquire_slot(&fx.pool, &idx) != 0, "acquire 2");
  KL_TEST_ASSERT(kl_binary_pool_acquire_slot(&fx.pool, &idx) != 0, "acquire 3");

  idx = 999;
  uint8_t* p = kl_binary_pool_acquire_slot(&fx.pool, &idx);
  KL_TEST_ASSERT(p == 0, "acquire on exhausted pool returns null");
  KL_TEST_EXPECT_EQ_INT((int)idx, -1);

  kl_test_bp_teardown(&fx);
}

void test_binary_pool_get_slot_before_acquire_returns_zero() {
  KL_TestBP fx = {0};
  kl_test_bp_setup(&fx, 4, sizeof(int64_t), _Alignof(int64_t));

  // No acquire has happened yet; items is still NULL.
  KL_TEST_ASSERT(kl_binary_pool_get_slot(&fx.pool, 0) == 0, "get returns 0 before any acquire");

  kl_test_bp_teardown(&fx);
}

void test_binary_pool_get_slot_matches_acquire() {
  KL_TestBP fx = {0};
  kl_test_bp_setup(&fx, 4, sizeof(int64_t), _Alignof(int64_t));

  int64_t idx = -1;
  uint8_t* p = kl_binary_pool_acquire_slot(&fx.pool, &idx);
  uint8_t* g = kl_binary_pool_get_slot(&fx.pool, idx);
  KL_TEST_ASSERT(p == g, "get_slot returns the same pointer as acquire");

  kl_test_bp_teardown(&fx);
}

void test_binary_pool_release_then_reacquire() {
  KL_TestBP fx = {0};
  kl_test_bp_setup(&fx, 2, sizeof(int64_t), _Alignof(int64_t));

  int64_t i0 = -1, i1 = -1;
  uint8_t* p0 = kl_binary_pool_acquire_slot(&fx.pool, &i0);
  uint8_t* p1 = kl_binary_pool_acquire_slot(&fx.pool, &i1);
  KL_TEST_ASSERT(p0 != 0 && p1 != 0, "both acquires succeed");

  // Pool is now exhausted.
  int64_t junk = -1;
  KL_TEST_ASSERT(kl_binary_pool_acquire_slot(&fx.pool, &junk) == 0, "exhausted");

  kl_binary_pool_release_slot(&fx.pool, i0);
  KL_TEST_ASSERT(fx.pool.taken_pos[i0] == -1, "taken_pos cleared on release");

  int64_t i2 = -1;
  uint8_t* p2 = kl_binary_pool_acquire_slot(&fx.pool, &i2);
  KL_TEST_ASSERT(p2 != 0, "reacquire after release succeeds");
  KL_TEST_EXPECT_EQ_INT((int)i2, (int)i0);
  KL_TEST_ASSERT(p2 == p0, "reacquired slot points to the same memory");

  kl_test_bp_teardown(&fx);
}

void test_binary_pool_release_unacquired_is_noop() {
  KL_TestBP fx = {0};
  kl_test_bp_setup(&fx, 4, sizeof(int64_t), _Alignof(int64_t));

  int64_t idx = -1;
  kl_binary_pool_acquire_slot(&fx.pool, &idx);
  // free_indices has 3 entries (count=4, one acquired).
  int64_t free_before = fx.pool.free_indices.length;

  // Pick an index that wasn't acquired and try to release it.
  int64_t other = (idx == 0) ? 1 : 0;
  kl_binary_pool_release_slot(&fx.pool, other);
  KL_TEST_EXPECT_EQ_INT((int)fx.pool.free_indices.length, (int)free_before);

  kl_test_bp_teardown(&fx);
}

void test_binary_pool_release_double_is_noop() {
  KL_TestBP fx = {0};
  kl_test_bp_setup(&fx, 4, sizeof(int64_t), _Alignof(int64_t));

  int64_t idx = -1;
  kl_binary_pool_acquire_slot(&fx.pool, &idx);
  int64_t free_after_acquire = fx.pool.free_indices.length;

  kl_binary_pool_release_slot(&fx.pool, idx);
  int64_t free_after_first_release = fx.pool.free_indices.length;
  KL_TEST_EXPECT_EQ_INT((int)free_after_first_release, (int)(free_after_acquire + 1));

  // Releasing again must not double-push into free_indices.
  kl_binary_pool_release_slot(&fx.pool, idx);
  KL_TEST_EXPECT_EQ_INT((int)fx.pool.free_indices.length, (int)free_after_first_release);

  kl_test_bp_teardown(&fx);
}

void test_binary_pool_release_before_acquire_is_safe() {
  KL_TestBP fx = {0};
  kl_test_bp_setup(&fx, 4, sizeof(int64_t), _Alignof(int64_t));

  // items is still NULL; release must short-circuit cleanly.
  kl_binary_pool_release_slot(&fx.pool, 0);
  KL_TEST_ASSERT(fx.pool.items == 0, "release before acquire does not allocate");

  kl_test_bp_teardown(&fx);
}

void test_binary_pool_exhaust_then_release_all() {
  KL_TestBP fx = {0};
  kl_test_bp_setup(&fx, 6, sizeof(int64_t), _Alignof(int64_t));

  int64_t idxs[6];
  for (int i = 0; i < 6; i++) {
    KL_TEST_ASSERT(kl_binary_pool_acquire_slot(&fx.pool, &idxs[i]) != 0, "acquire");
  }
  int64_t junk = -1;
  KL_TEST_ASSERT(kl_binary_pool_acquire_slot(&fx.pool, &junk) == 0, "exhausted");

  for (int i = 0; i < 6; i++) {
    kl_binary_pool_release_slot(&fx.pool, idxs[i]);
  }
  KL_TEST_EXPECT_EQ_INT((int)fx.pool.free_indices.length, 6);

  // All slots reacquirable.
  for (int i = 0; i < 6; i++) {
    KL_TEST_ASSERT(kl_binary_pool_acquire_slot(&fx.pool, &idxs[i]) != 0, "reacquire");
  }

  kl_test_bp_teardown(&fx);
}

void test_binary_pool_reset_marks_all_free() {
  KL_TestBP fx = {0};
  kl_test_bp_setup(&fx, 4, sizeof(int64_t), _Alignof(int64_t));

  int64_t idx = -1;
  int64_t* p = (int64_t*)kl_binary_pool_acquire_slot(&fx.pool, &idx);
  *p = 0x7fffffffffffffffLL;
  kl_binary_pool_acquire_slot(&fx.pool, &idx);
  KL_TEST_EXPECT_EQ_INT((int)fx.pool.free_indices.length, 2);

  kl_binary_pool_reset(&fx.pool);
  KL_TEST_EXPECT_EQ_INT((int)fx.pool.free_indices.length, 4);
  // Slot memory zeroed.
  int64_t* g = (int64_t*)kl_binary_pool_get_slot(&fx.pool, 0);
  KL_TEST_EXPECT_EQ_INT((int)*g, 0);
  // taken_pos cleared.
  for (int i = 0; i < 4; i++) {
    KL_TEST_ASSERT(fx.pool.taken_pos[i] == -1, "taken_pos cleared on reset");
  }

  // After reset all slots reacquirable.
  for (int i = 0; i < 4; i++) {
    int64_t j = -1;
    KL_TEST_ASSERT(kl_binary_pool_acquire_slot(&fx.pool, &j) != 0, "reacquire after reset");
  }

  kl_test_bp_teardown(&fx);
}

void test_binary_pool_reset_before_acquire_is_safe() {
  KL_TestBP fx = {0};
  kl_test_bp_setup(&fx, 4, sizeof(int64_t), _Alignof(int64_t));

  // Reset before any acquire must not pre-fill free_indices (which would
  // collide with ensure_slots' assertion on the next acquire).
  kl_binary_pool_reset(&fx.pool);
  KL_TEST_EXPECT_EQ_INT((int)fx.pool.free_indices.length, 0);

  int64_t idx = -1;
  KL_TEST_ASSERT(kl_binary_pool_acquire_slot(&fx.pool, &idx) != 0, "acquire after early reset");

  kl_test_bp_teardown(&fx);
}

void test_binary_pool_count_one_pool() {
  KL_TestBP fx = {0};
  kl_test_bp_setup(&fx, 1, sizeof(int64_t), _Alignof(int64_t));

  int64_t i0 = -1, i1 = -1;
  uint8_t* p0 = kl_binary_pool_acquire_slot(&fx.pool, &i0);
  uint8_t* p1 = kl_binary_pool_acquire_slot(&fx.pool, &i1);
  KL_TEST_ASSERT(p0 != 0, "single-slot pool acquires once");
  KL_TEST_ASSERT(p1 == 0, "single-slot pool refuses second acquire");
  KL_TEST_EXPECT_EQ_INT((int)i0, 0);

  kl_binary_pool_release_slot(&fx.pool, i0);
  uint8_t* p2 = kl_binary_pool_acquire_slot(&fx.pool, &i1);
  KL_TEST_ASSERT(p2 == p0, "reacquire yields same single slot");

  kl_test_bp_teardown(&fx);
}

void test_binary_pool_handle_resolve_and_get() {
  KL_TestBP fx = {0};
  kl_test_bp_setup(&fx, 4, sizeof(int64_t), _Alignof(int64_t));

  uint8_t* p = 0;
  int64_t h = kl_binary_pool_acquire(&fx.pool, &p);
  KL_TEST_ASSERT(h >= 0 && p != 0, "acquire returns a valid handle and pointer");

  int64_t idx = kl_binary_pool_resolve(&fx.pool, h);
  KL_TEST_ASSERT(idx == kl_binary_pool_handle_index(h), "resolve matches the encoded index");
  KL_TEST_ASSERT(kl_binary_pool_get(&fx.pool, h) == p, "get-by-handle matches the acquire pointer");
  KL_TEST_ASSERT(kl_binary_pool_get_slot(&fx.pool, idx) == p, "resolved index points at the same storage");
  KL_TEST_ASSERT(kl_binary_pool_has(&fx.pool, h), "fresh handle reports present");

  kl_test_bp_teardown(&fx);
}

void test_binary_pool_handle_invalid_after_release() {
  KL_TestBP fx = {0};
  kl_test_bp_setup(&fx, 4, sizeof(int64_t), _Alignof(int64_t));

  int64_t h = kl_binary_pool_acquire(&fx.pool, 0);
  KL_TEST_ASSERT(h >= 0, "acquire ok");

  kl_binary_pool_release(&fx.pool, h);
  KL_TEST_ASSERT(!kl_binary_pool_has(&fx.pool, h), "released handle reports absent");
  KL_TEST_ASSERT(kl_binary_pool_resolve(&fx.pool, h) == -1, "released handle resolves to -1");
  KL_TEST_ASSERT(kl_binary_pool_get(&fx.pool, h) == 0, "released handle gets null");

  kl_test_bp_teardown(&fx);
}

// The core ABA case: a single-slot pool guarantees the freed index is recycled,
// so the old handle would alias the new occupant if it weren't generation-checked.
void test_binary_pool_handle_rejects_recycled_slot() {
  KL_TestBP fx = {0};
  kl_test_bp_setup(&fx, 1, sizeof(int64_t), _Alignof(int64_t));

  uint8_t* p0 = 0;
  int64_t h0 = kl_binary_pool_acquire(&fx.pool, &p0);
  KL_TEST_ASSERT(h0 >= 0 && p0 != 0, "first acquire returns a valid handle");
  KL_TEST_ASSERT(kl_binary_pool_handle_index(h0) == 0, "slot index is 0 in a one-slot pool");

  kl_binary_pool_release(&fx.pool, h0);

  uint8_t* p1 = 0;
  int64_t h1 = kl_binary_pool_acquire(&fx.pool, &p1);
  KL_TEST_ASSERT(h1 >= 0 && p1 == p0, "slot is recycled into the same storage");
  KL_TEST_ASSERT(h1 != h0, "recycled slot yields a distinct handle");

  KL_TEST_ASSERT(!kl_binary_pool_has(&fx.pool, h0), "stale handle rejected after reuse");
  KL_TEST_ASSERT(kl_binary_pool_resolve(&fx.pool, h0) == -1, "stale handle resolves to -1 after reuse");
  KL_TEST_ASSERT(kl_binary_pool_get(&fx.pool, h0) == 0, "stale handle gets null after reuse");
  KL_TEST_ASSERT(kl_binary_pool_has(&fx.pool, h1), "fresh handle still valid");

  kl_test_bp_teardown(&fx);
}

// Releasing a stale handle must not evict the live occupant that recycled its slot.
void test_binary_pool_handle_stale_release_is_noop() {
  KL_TestBP fx = {0};
  kl_test_bp_setup(&fx, 1, sizeof(int64_t), _Alignof(int64_t));

  int64_t h0 = kl_binary_pool_acquire(&fx.pool, 0);
  kl_binary_pool_release(&fx.pool, h0);
  int64_t h1 = kl_binary_pool_acquire(&fx.pool, 0);

  kl_binary_pool_release(&fx.pool, h0); // stale — must be ignored
  KL_TEST_ASSERT(kl_binary_pool_has(&fx.pool, h1), "live handle survives a stale release");

  kl_test_bp_teardown(&fx);
}

void test_binary_pool_handle_invalid_after_reset() {
  KL_TestBP fx = {0};
  kl_test_bp_setup(&fx, 2, sizeof(int64_t), _Alignof(int64_t));

  int64_t h0 = kl_binary_pool_acquire(&fx.pool, 0);
  kl_binary_pool_reset(&fx.pool);
  KL_TEST_ASSERT(!kl_binary_pool_has(&fx.pool, h0), "handle invalid after reset");

  // Generations are kept monotonic across reset, so a post-reset acquire can
  // never mint a handle equal to one handed out before the reset.
  int64_t h1 = kl_binary_pool_acquire(&fx.pool, 0);
  KL_TEST_ASSERT(h1 != h0, "post-reset handle distinct from pre-reset handle");
  KL_TEST_ASSERT(!kl_binary_pool_has(&fx.pool, h0), "old handle stays invalid after reuse");

  kl_test_bp_teardown(&fx);
}

// White-box: jam a slot's generation to the field max so the next acquire wraps.
// The handle must stay non-negative (sign bit reserved) and still resolve.
void test_binary_pool_handle_generation_wraps() {
  KL_TestBP fx = {0};
  kl_test_bp_setup(&fx, 1, sizeof(int64_t), _Alignof(int64_t));

  // First acquire allocates the internal arrays so we can poke `generations`.
  int64_t h = kl_binary_pool_acquire(&fx.pool, 0);
  KL_TEST_ASSERT(h >= 0, "initial handle non-negative");
  kl_binary_pool_release(&fx.pool, h);

  fx.pool.generations[0] = KL_BINARY_POOL_HANDLE_GENERATION_MASK; // 2^31 - 1

  // The high-generation acquire (just below the wrap) is still non-negative.
  int64_t hmax = kl_binary_pool_acquire(&fx.pool, 0);
  KL_TEST_ASSERT(hmax >= 0, "max-generation handle stays non-negative");
  KL_TEST_ASSERT(kl_binary_pool_has(&fx.pool, hmax), "max-generation handle resolves");
  kl_binary_pool_release(&fx.pool, hmax);

  // Now force the wrap: generation goes 2^31-1 -> 0 modulo the field.
  fx.pool.generations[0] = KL_BINARY_POOL_HANDLE_GENERATION_MASK;
  int64_t hwrap = kl_binary_pool_acquire(&fx.pool, 0);
  KL_TEST_ASSERT(hwrap >= 0, "wrapped handle stays non-negative");
  KL_TEST_ASSERT(kl_binary_pool_handle_generation(hwrap) == 0, "generation wrapped to 0");
  KL_TEST_ASSERT(kl_binary_pool_has(&fx.pool, hwrap), "wrapped handle resolves");
  KL_TEST_ASSERT(kl_binary_pool_resolve(&fx.pool, hwrap) == 0, "wrapped handle resolves to slot 0");

  kl_test_bp_teardown(&fx);
}
