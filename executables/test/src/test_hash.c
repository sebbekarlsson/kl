#include <test/tests.h>
#include <kl/hash/hash.h>

static bool is_lower_hex(char c) {
  return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'f');
}

// Validates a single string against the RFC 4122 v4 layout:
//   xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx   (y in {8,9,a,b})
static bool is_valid_uuidv4(KL_StrConst s) {
  if (s.length != 36) return false;
  for (int64_t i = 0; i < s.length; i++) {
    char c = s.value[i];
    if (i == 8 || i == 13 || i == 18 || i == 23) {
      if (c != '-') return false;
    } else if (i == 14) {
      if (c != '4') return false;            // version nibble
    } else if (i == 19) {
      if (!(c == '8' || c == '9' || c == 'a' || c == 'b')) return false;  // variant
    } else {
      if (!is_lower_hex(c)) return false;
    }
  }
  return true;
}

void test_hash_uuidv4_format() {
  uint32_t seed = 0x12345678U;
  KL_StrBuffer buff = KL_STR_BUFFER_STACK_ALLOC(64);

  // Generate a batch and check every one is structurally valid.
  for (int i = 0; i < 256; i++) {
    kl_str_buffer_clear(&buff);
    KL_TEST_EXPECT_EXPR(kl_hash_uint32_next_string_uuidv4(&seed, &buff));
    KL_TEST_EXPECT_EQ_INT((int)buff.length, 36);
    KL_TEST_EXPECT_EXPR(is_valid_uuidv4(KL_STR_BUFFER_TO_STR_CONST(buff)));
  }
}

void test_hash_uuidv4_deterministic() {
  KL_StrBuffer a = KL_STR_BUFFER_STACK_ALLOC(64);
  KL_StrBuffer b = KL_STR_BUFFER_STACK_ALLOC(64);

  uint32_t seed_a = 0xdeadbeefU;
  uint32_t seed_b = 0xdeadbeefU;
  kl_hash_uint32_next_string_uuidv4(&seed_a, &a);
  kl_hash_uint32_next_string_uuidv4(&seed_b, &b);

  // Same starting seed must produce identical output and advance the seed
  // identically.
  KL_TEST_EXPECT_EXPR(kl_str_const_eq(KL_STR_BUFFER_TO_STR_CONST(a), KL_STR_BUFFER_TO_STR_CONST(b)));
  KL_TEST_EXPECT_EXPR(seed_a == seed_b);
}

void test_hash_uuidv4_distinct() {
  uint32_t seed = 0x0badf00dU;
  KL_StrBuffer prev = KL_STR_BUFFER_STACK_ALLOC(64);
  KL_StrBuffer cur = KL_STR_BUFFER_STACK_ALLOC(64);

  kl_hash_uint32_next_string_uuidv4(&seed, &prev);
  for (int i = 0; i < 64; i++) {
    kl_str_buffer_clear(&cur);
    kl_hash_uint32_next_string_uuidv4(&seed, &cur);
    // Consecutive draws from the same advancing seed should differ.
    KL_TEST_EXPECT_EXPR(!kl_str_const_eq(KL_STR_BUFFER_TO_STR_CONST(prev), KL_STR_BUFFER_TO_STR_CONST(cur)));
    kl_str_buffer_clear(&prev);
    kl_str_buffer_append(&prev, KL_STR_BUFFER_TO_STR_CONST(cur));
  }
}

void test_hash_uuidv4_hex_uses_digits() {
  // Regression: the 'x' nibbles must span the full 0-9a-f alphabet, not just
  // a-f. Across a large batch, expect to see at least one decimal digit and at
  // least one a-f letter somewhere in the random 'x' positions.
  uint32_t seed = 0xcafebabeU;
  KL_StrBuffer buff = KL_STR_BUFFER_STACK_ALLOC(64);
  bool saw_digit = false;
  bool saw_alpha = false;

  for (int i = 0; i < 256 && !(saw_digit && saw_alpha); i++) {
    kl_str_buffer_clear(&buff);
    kl_hash_uint32_next_string_uuidv4(&seed, &buff);
    for (int64_t j = 0; j < buff.length; j++) {
      // Skip the fixed positions (dashes, version, variant) so we only sample
      // the random 'x' nibbles.
      if (j == 8 || j == 13 || j == 18 || j == 23 || j == 14 || j == 19) continue;
      char c = buff.data[j];
      if (c >= '0' && c <= '9') saw_digit = true;
      if (c >= 'a' && c <= 'f') saw_alpha = true;
    }
  }

  KL_TEST_EXPECT_EXPR(saw_digit);
  KL_TEST_EXPECT_EXPR(saw_alpha);
}
