#include <test/tests.h>

/* ===================== vecn (allocator-backed) test helpers ===================== */

#define VECN_EPS 1e-5f

// Every allocating vecn function must thread the source vector's allocator
// through to its result, so callers can keep allocating/chaining off the
// returned vector. These tests pin that contract down explicitly.

static KL_Allocator vecn_make_allocator(void) {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 4096 });
  return a;
}

static void expect_items(KL_VecN_f32 v, const float* expected, int64_t length) {
  KL_TEST_EXPECT_EQ_INT(v.length, length);
  for (int64_t i = 0; i < length; i++) {
    KL_TEST_EXPECT_EQ_FLOAT(v.items[i], expected[i], VECN_EPS);
  }
}

/* ===================== create / copy ===================== */

void test_vecn_f32_create() {
  KL_Allocator a = vecn_make_allocator();

  float xs[] = { 1.0f, 2.0f, 3.0f };
  KL_VecN_f32 v = kl_vecn_f32_create(xs, 3, &a);

  KL_TEST_ASSERT(v.items != 0, "create allocates a backing buffer");
  KL_TEST_ASSERT(v.items != xs, "create copies the source data, does not alias it");
  KL_TEST_ASSERT(v.allocator == &a, "create stores the allocator on the result");
  expect_items(v, xs, 3);

  kl_allocator_destroy(&a);
}

void test_vecn_f32_create_null_items() {
  KL_Allocator a = vecn_make_allocator();

  // A null `items` allocates without copying; the allocator zero-inits memory.
  KL_VecN_f32 v = kl_vecn_f32_create(0, 4, &a);

  KL_TEST_ASSERT(v.items != 0, "create still allocates when items is null");
  KL_TEST_ASSERT(v.allocator == &a, "create stores the allocator even with null items");
  float zeros[] = { 0.0f, 0.0f, 0.0f, 0.0f };
  expect_items(v, zeros, 4);

  kl_allocator_destroy(&a);
}

void test_vecn_f32_copy() {
  KL_Allocator a = vecn_make_allocator();

  float xs[] = { 5.0f, 6.0f, 7.0f };
  KL_VecN_f32 src = kl_vecn_f32_create(xs, 3, &a);
  KL_VecN_f32 dst = kl_vecn_f32_copy(src);

  KL_TEST_ASSERT(dst.items != src.items, "copy produces an independent buffer");
  KL_TEST_ASSERT(dst.allocator == &a, "copy carries the source allocator");
  expect_items(dst, xs, 3);

  // Mutating the copy must not touch the source.
  dst.items[0] = 99.0f;
  KL_TEST_EXPECT_EQ_FLOAT(src.items[0], 5.0f, VECN_EPS);

  kl_allocator_destroy(&a);
}

/* ===================== unary ops ===================== */

void test_vecn_f32_unit() {
  KL_Allocator a = vecn_make_allocator();

  float xs[] = { 3.0f, 4.0f };
  KL_VecN_f32 v = kl_vecn_f32_create(xs, 2, &a);
  KL_VecN_f32 u = kl_vecn_f32_unit(v);

  float expected[] = { 0.6f, 0.8f };
  expect_items(u, expected, 2);
  KL_TEST_EXPECT_EQ_FLOAT(kl_vecn_f32_mag(u), 1.0f, VECN_EPS);
  KL_TEST_ASSERT(u.allocator == &a, "unit carries the source allocator");

  // The source vector is left untouched (non-_ip is immutable on its input).
  expect_items(v, xs, 2);

  kl_allocator_destroy(&a);
}

void test_vecn_f32_scale() {
  KL_Allocator a = vecn_make_allocator();

  float xs[] = { 1.0f, -2.0f, 3.0f };
  KL_VecN_f32 v = kl_vecn_f32_create(xs, 3, &a);
  KL_VecN_f32 s = kl_vecn_f32_scale(v, 2.0f);

  float expected[] = { 2.0f, -4.0f, 6.0f };
  expect_items(s, expected, 3);
  KL_TEST_ASSERT(s.allocator == &a, "scale carries the source allocator");
  expect_items(v, xs, 3);

  kl_allocator_destroy(&a);
}

/* ===================== binary ops ===================== */

// The binary ops build their result from the LEFT operand, so the result must
// carry `a`'s allocator regardless of where `b` came from.

void test_vecn_f32_add() {
  KL_Allocator a1 = vecn_make_allocator();
  KL_Allocator a2 = vecn_make_allocator();

  float xs[] = { 1.0f, 2.0f, 3.0f };
  float ys[] = { 4.0f, 5.0f, 6.0f };
  KL_VecN_f32 a = kl_vecn_f32_create(xs, 3, &a1);
  KL_VecN_f32 b = kl_vecn_f32_create(ys, 3, &a2);

  KL_VecN_f32 r = kl_vecn_f32_add(a, b);

  float expected[] = { 5.0f, 7.0f, 9.0f };
  expect_items(r, expected, 3);
  KL_TEST_ASSERT(r.allocator == &a1, "add result follows the left operand's allocator");
  // Operands untouched.
  expect_items(a, xs, 3);
  expect_items(b, ys, 3);

  kl_allocator_destroy(&a1);
  kl_allocator_destroy(&a2);
}

void test_vecn_f32_sub() {
  KL_Allocator a1 = vecn_make_allocator();
  KL_Allocator a2 = vecn_make_allocator();

  float xs[] = { 5.0f, 7.0f, 9.0f };
  float ys[] = { 1.0f, 2.0f, 3.0f };
  KL_VecN_f32 a = kl_vecn_f32_create(xs, 3, &a1);
  KL_VecN_f32 b = kl_vecn_f32_create(ys, 3, &a2);

  KL_VecN_f32 r = kl_vecn_f32_sub(a, b);

  float expected[] = { 4.0f, 5.0f, 6.0f };
  expect_items(r, expected, 3);
  KL_TEST_ASSERT(r.allocator == &a1, "sub result follows the left operand's allocator");
  expect_items(a, xs, 3);
  expect_items(b, ys, 3);

  kl_allocator_destroy(&a1);
  kl_allocator_destroy(&a2);
}

void test_vecn_f32_mul() {
  KL_Allocator a1 = vecn_make_allocator();
  KL_Allocator a2 = vecn_make_allocator();

  float xs[] = { 2.0f, 3.0f, 4.0f };
  float ys[] = { 5.0f, 6.0f, 7.0f };
  KL_VecN_f32 a = kl_vecn_f32_create(xs, 3, &a1);
  KL_VecN_f32 b = kl_vecn_f32_create(ys, 3, &a2);

  KL_VecN_f32 r = kl_vecn_f32_mul(a, b);

  float expected[] = { 10.0f, 18.0f, 28.0f };
  expect_items(r, expected, 3);
  KL_TEST_ASSERT(r.allocator == &a1, "mul result follows the left operand's allocator");
  expect_items(a, xs, 3);
  expect_items(b, ys, 3);

  kl_allocator_destroy(&a1);
  kl_allocator_destroy(&a2);
}

void test_vecn_f32_div() {
  KL_Allocator a1 = vecn_make_allocator();
  KL_Allocator a2 = vecn_make_allocator();

  float xs[] = { 10.0f, 18.0f, 28.0f };
  float ys[] = { 5.0f, 6.0f, 7.0f };
  KL_VecN_f32 a = kl_vecn_f32_create(xs, 3, &a1);
  KL_VecN_f32 b = kl_vecn_f32_create(ys, 3, &a2);

  KL_VecN_f32 r = kl_vecn_f32_div(a, b);

  float expected[] = { 2.0f, 3.0f, 4.0f };
  expect_items(r, expected, 3);
  KL_TEST_ASSERT(r.allocator == &a1, "div result follows the left operand's allocator");
  expect_items(a, xs, 3);
  expect_items(b, ys, 3);

  kl_allocator_destroy(&a1);
  kl_allocator_destroy(&a2);
}
