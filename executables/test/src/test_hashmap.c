#include <test/tests.h>

static KL_HashmapConfig kl_test_int64_hashmap_default_cfg(void) {
  return (KL_HashmapConfig){
    .capacity = 16,
    .collision_capacity = 16,
    .track_used_slots = false,
  };
}

void test_int64_hashmap_init_destroy() {
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

void test_int64_hashmap_double_init_is_noop() {
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

void test_int64_hashmap_capacity_rounds_to_power_of_two() {
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

void test_int64_hashmap_set_get_basic() {
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

void test_int64_hashmap_get_missing_returns_zero() {
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

void test_int64_hashmap_set_overwrite_updates_value() {
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

void test_int64_hashmap_set_returns_valid_slot_index() {
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

void test_int64_hashmap_has_before_and_after() {
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

void test_int64_hashmap_unset_removes_entry() {
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

void test_int64_hashmap_unset_missing_returns_zero() {
  KL_int64_Hashmap map = {0};
  kl_int64_hashmap_init(&map, kl_test_int64_hashmap_default_cfg());
  int64_t k = 1234;
  // Unsetting an absent key is a no-op that returns 0.
  KL_TEST_EXPECT_EQ_INT(kl_int64_hashmap_unset(&map, KL_BLOB_AUTO(k)), 0);
  kl_int64_hashmap_destroy(&map);
}

void test_int64_hashmap_unset_then_reset_reinserts() {
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

void test_int64_hashmap_get_ptr_allows_mutation() {
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

void test_int64_hashmap_get_ptr_missing_returns_zero() {
  KL_int64_Hashmap map = {0};
  kl_int64_hashmap_init(&map, kl_test_int64_hashmap_default_cfg());
  int64_t k = 0xBEEF;
  int64_t* p = (int64_t*)0xDEAD;
  KL_TEST_EXPECT_EQ_INT(kl_int64_hashmap_get_ptr(&map, KL_BLOB_AUTO(k), &p), 0);
  kl_int64_hashmap_destroy(&map);
}

void test_int64_hashmap_hash_is_deterministic() {
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

void test_int64_hashmap_collisions_with_tiny_capacity() {
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

void test_int64_hashmap_many_entries_round_trip() {
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

void test_int64_hashmap_unset_in_collision_chain() {
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

void test_int64_hashmap_string_keys_fnv_path() {
  // Keys longer than sizeof(int64_t) bytes take the FNV-1a hashing path.
  KL_int64_Hashmap map = {0};
  KL_HashmapConfig cfg = (KL_HashmapConfig){
    .capacity = 16,
    .collision_capacity = 16,
    .track_used_slots = false,
  };
  kl_int64_hashmap_init(&map, cfg);

  const char* keys[] = {
    "alpha_string_key", "beta_string_key", "gamma_string_key",
    "delta_string_key", "epsilon_string_key", "zeta_string_key"
  };
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
  KL_Blob miss = { .value = (uint8_t*)"omega_string_key", .size = 16 };
  KL_TEST_ASSERT(!kl_int64_hashmap_has(&map, miss), "uninserted string key missing");

  kl_int64_hashmap_destroy(&map);
}

void test_int64_hashmap_keys_with_same_prefix_differ() {
  // Distinguishes by both bytes and length, even when one is a prefix of another.
  KL_int64_Hashmap map = {0};
  KL_HashmapConfig cfg = (KL_HashmapConfig){
    .capacity = 8,
    .collision_capacity = 8,
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

void test_int64_hashmap_get_keys_lists_all_set_keys() {
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

void test_int64_hashmap_get_keys_excludes_unset() {
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

void test_int64_hashmap_used_slots_tracking() {
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

void test_int64_hashmap_get_used_slots_returns_set_only() {
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

void test_int64_hashmap_reset_clears_entries() {
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

void test_int64_hashmap_serialize_deserialize_round_trip() {
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

void test_int64_hashmap_deserialize_rejects_bad_magic() {
  // Writing arbitrary bytes where the magic header is expected must be rejected.
  FILE* fp = tmpfile();
  KL_TEST_ASSERT(fp != 0, "tmpfile non-null");
  uint32_t bogus = 0xCAFEBABE;
  fwrite(&bogus, sizeof(bogus), 1, fp);
  rewind(fp);

  KL_int64_Hashmap dst = {0};
  int rc = kl_int64_hashmap_deserialize_from_file(&dst, fp);

  KL_TEST_EXPECT_EQ_INT(rc, 0);
  KL_TEST_ASSERT(!dst.initialized, "dst left uninitialized on bad magic");
  fclose(fp);
}

void test_int64_hashmap_zero_value_is_distinguishable() {
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

/* Custom hash fn that always returns 0, forcing every key into the same home
   bucket regardless of its bytes. Exercises the override + collision chain. */
static uint64_t kl_test_constant_hash(KL_Blob key, void* user_ptr) {
  (void)key;
  (void)user_ptr;
  return 0;
}

/* Custom hash fn that bumps an int64 counter through user_ptr each call, so a
   test can prove the map actually routed hashing through the config fn. */
static uint64_t kl_test_counting_hash(KL_Blob key, void* user_ptr) {
  int64_t* calls = (int64_t*)user_ptr;
  if (calls != 0) *calls += 1;
  /* FNV-1a so distinct keys still spread across buckets. */
  uint64_t h = 14695981039346656037ULL;
  for (int64_t i = 0; i < key.size; i++) {
    h ^= (uint8_t)key.value[i];
    h *= 1099511628211ULL;
  }
  return h;
}

void test_int64_hashmap_custom_hash_is_invoked() {
  // The config hash fn must be called for every hashing operation, and the
  // user_ptr must be threaded through to it unchanged.
  int64_t calls = 0;
  KL_int64_Hashmap map = {0};
  KL_HashmapConfig cfg = kl_test_int64_hashmap_default_cfg();
  cfg.hash.fn = kl_test_counting_hash;
  cfg.hash.user_ptr = &calls;
  kl_int64_hashmap_init(&map, cfg);

  int64_t k = 7;
  KL_HashmapHash h = kl_int64_hashmap_get_hash(&map, KL_BLOB_AUTO(k));
  KL_TEST_ASSERT(calls == 1, "custom hash fn invoked once for get_hash");
  // Index must still be folded into the table via the capacity mask.
  KL_TEST_ASSERT(h.index >= 0 && h.index < map.capacity_pow2, "index in [0, pow2)");
  KL_TEST_ASSERT((uint64_t)h.index == (h.hash & map.capacity_mask), "index is hash & mask");

  calls = 0;
  kl_int64_hashmap_set(&map, KL_BLOB_AUTO(k), 99);
  KL_TEST_ASSERT(calls >= 1, "custom hash fn invoked on set");

  calls = 0;
  int64_t out = 0;
  KL_TEST_EXPECT_EQ_INT(kl_int64_hashmap_get(&map, KL_BLOB_AUTO(k), &out), 1);
  KL_TEST_EXPECT_EQ_INT((int)out, 99);
  KL_TEST_ASSERT(calls >= 1, "custom hash fn invoked on get");

  kl_int64_hashmap_destroy(&map);
}

void test_int64_hashmap_custom_hash_constant_forces_collisions() {
  // A constant hash sends every key to slot 0, yet the map must still keep all
  // keys distinct (compare falls back to the raw key bytes, not the hash).
  KL_int64_Hashmap map = {0};
  KL_HashmapConfig cfg = kl_test_int64_hashmap_default_cfg();
  cfg.hash.fn = kl_test_constant_hash;
  kl_int64_hashmap_init(&map, cfg);

  const int N = 32;
  for (int i = 0; i < N; i++) {
    int64_t k = i + 1;
    KL_TEST_ASSERT(kl_int64_hashmap_set(&map, KL_BLOB_AUTO(k), (int64_t)(k * 10)) >= 0,
                   "set under constant hash");
  }
  // Every key shares one home bucket, so all but the first insert collide.
  KL_TEST_EXPECT_EQ_INT((int)map.n_collisions, N - 1);

  for (int i = 0; i < N; i++) {
    int64_t k = i + 1;
    int64_t out = 0;
    KL_TEST_EXPECT_EQ_INT(kl_int64_hashmap_get(&map, KL_BLOB_AUTO(k), &out), 1);
    KL_TEST_EXPECT_EQ_INT((int)out, (int)(k * 10));
  }
  // A never-inserted key still misses despite hashing to the same bucket.
  int64_t missing = N + 100;
  KL_TEST_ASSERT(!kl_int64_hashmap_has(&map, KL_BLOB_AUTO(missing)), "absent key still misses");

  kl_int64_hashmap_destroy(&map);
}

void test_int64_hashmap_custom_hash_overrides_builtin() {
  // The same key under a custom hash must land where the custom hash dictates,
  // not where the built-in murmur path would have put it.
  int64_t k = 123456;

  KL_int64_Hashmap builtin = {0};
  kl_int64_hashmap_init(&builtin, kl_test_int64_hashmap_default_cfg());
  KL_HashmapHash hb = kl_int64_hashmap_get_hash(&builtin, KL_BLOB_AUTO(k));

  KL_int64_Hashmap custom = {0};
  KL_HashmapConfig cfg = kl_test_int64_hashmap_default_cfg();
  cfg.hash.fn = kl_test_constant_hash;
  kl_int64_hashmap_init(&custom, cfg);
  KL_HashmapHash hc = kl_int64_hashmap_get_hash(&custom, KL_BLOB_AUTO(k));

  // Constant hash yields hash 0; the built-in murmur path is overwhelmingly
  // unlikely to also produce 0 for this key.
  KL_TEST_ASSERT(hc.hash == 0, "custom constant hash returns 0");
  KL_TEST_ASSERT(hb.hash != hc.hash, "custom hash overrides built-in hashing");

  kl_int64_hashmap_destroy(&builtin);
  kl_int64_hashmap_destroy(&custom);
}

void test_int64_hashmap_negative_and_extreme_keys() {
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
