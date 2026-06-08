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

/* ===================== distance ===================== */

void test_vecn_f32_distance() {
  KL_Allocator a = vecn_make_allocator();

  float xs[] = { 0.0f, 0.0f };
  float ys[] = { 3.0f, 4.0f };
  KL_VecN_f32 v = kl_vecn_f32_create(xs, 2, &a);
  KL_VecN_f32 w = kl_vecn_f32_create(ys, 2, &a);

  // sqrt(3^2 + 4^2) = 5.
  KL_TEST_EXPECT_EQ_FLOAT(kl_vecn_f32_distance(v, w), 5.0f, VECN_EPS);
  // Distance from a vector to itself is 0.
  KL_TEST_EXPECT_EQ_FLOAT(kl_vecn_f32_distance(w, w), 0.0f, VECN_EPS);
  // Symmetric in its arguments.
  KL_TEST_EXPECT_EQ_FLOAT(kl_vecn_f32_distance(v, w),
                          kl_vecn_f32_distance(w, v), VECN_EPS);

  // Operands are not mutated.
  expect_items(v, xs, 2);
  expect_items(w, ys, 2);

  kl_allocator_destroy(&a);
}

void test_vecn_f32_distance_equals_mag_of_sub() {
  KL_Allocator a = vecn_make_allocator();

  float xs[] = { 1.0f, -2.0f, 3.0f, 0.5f };
  float ys[] = { 4.0f,  2.0f, -1.0f, 2.5f };
  KL_VecN_f32 v = kl_vecn_f32_create(xs, 4, &a);
  KL_VecN_f32 w = kl_vecn_f32_create(ys, 4, &a);

  // distance(a, b) must equal magnitude(sub(b, a)).
  KL_VecN_f32 diff = kl_vecn_f32_sub(w, v);
  KL_TEST_EXPECT_EQ_FLOAT(kl_vecn_f32_distance(v, w), kl_vecn_f32_mag(diff), VECN_EPS);

  kl_allocator_destroy(&a);
}

/* ===================== cosine distance ===================== */

void test_vecn_f32_cosine_distance() {
  KL_Allocator a = vecn_make_allocator();

  float xs[] = { 1.0f, 0.0f };
  float same[] = { 3.0f, 0.0f };   // same direction, different magnitude
  float orth[] = { 0.0f, 5.0f };   // orthogonal
  float opp[]  = { -2.0f, 0.0f };  // opposite direction
  float diag[] = { 1.0f, 1.0f };   // 45 degrees from xs

  KL_VecN_f32 v   = kl_vecn_f32_create(xs, 2, &a);
  KL_VecN_f32 vs  = kl_vecn_f32_create(same, 2, &a);
  KL_VecN_f32 vo  = kl_vecn_f32_create(orth, 2, &a);
  KL_VecN_f32 vop = kl_vecn_f32_create(opp, 2, &a);
  KL_VecN_f32 vd  = kl_vecn_f32_create(diag, 2, &a);

  // Same direction -> distance 0 (magnitude is irrelevant).
  KL_TEST_EXPECT_EQ_FLOAT(kl_vecn_f32_cosine_distance(v, vs), 0.0f, VECN_EPS);
  // Orthogonal -> similarity 0 -> distance 1.
  KL_TEST_EXPECT_EQ_FLOAT(kl_vecn_f32_cosine_distance(v, vo), 1.0f, VECN_EPS);
  // Opposite -> similarity -1 -> distance 2.
  KL_TEST_EXPECT_EQ_FLOAT(kl_vecn_f32_cosine_distance(v, vop), 2.0f, VECN_EPS);
  // 45 degrees -> similarity cos(45) = 1/sqrt(2) -> distance 1 - 0.70710678.
  KL_TEST_EXPECT_EQ_FLOAT(kl_vecn_f32_cosine_distance(v, vd), 1.0f - 0.70710678f, VECN_EPS);
  // Symmetric in its arguments.
  KL_TEST_EXPECT_EQ_FLOAT(kl_vecn_f32_cosine_distance(vd, v),
                          kl_vecn_f32_cosine_distance(v, vd), VECN_EPS);

  kl_allocator_destroy(&a);
}

void test_vecn_f32_cosine_distance_zero_vector() {
  KL_Allocator a = vecn_make_allocator();

  float xs[] = { 1.0f, 2.0f, 3.0f };
  float zero[] = { 0.0f, 0.0f, 0.0f };
  KL_VecN_f32 v = kl_vecn_f32_create(xs, 3, &a);
  KL_VecN_f32 z = kl_vecn_f32_create(zero, 3, &a);

  // A zero vector has no direction; distance is defined as 1 (max) rather than NaN.
  KL_TEST_EXPECT_EQ_FLOAT(kl_vecn_f32_cosine_distance(v, z), 1.0f, VECN_EPS);
  KL_TEST_EXPECT_EQ_FLOAT(kl_vecn_f32_cosine_distance(z, z), 1.0f, VECN_EPS);

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

/* ===================== reductions ===================== */

void test_vecn_f32_mag_sq() {
  KL_Allocator a = vecn_make_allocator();

  float xs[] = { 3.0f, 4.0f };
  KL_VecN_f32 v = kl_vecn_f32_create(xs, 2, &a);

  // 3^2 + 4^2 = 25 (no sqrt).
  KL_TEST_EXPECT_EQ_FLOAT(kl_vecn_f32_mag_sq(v), 25.0f, VECN_EPS);
  // mag_sq is exactly mag squared.
  float m = kl_vecn_f32_mag(v);
  KL_TEST_EXPECT_EQ_FLOAT(kl_vecn_f32_mag_sq(v), m * m, VECN_EPS);

  float zero[] = { 0.0f, 0.0f, 0.0f };
  KL_VecN_f32 z = kl_vecn_f32_create(zero, 3, &a);
  KL_TEST_EXPECT_EQ_FLOAT(kl_vecn_f32_mag_sq(z), 0.0f, VECN_EPS);

  kl_allocator_destroy(&a);
}

void test_vecn_f32_sum() {
  KL_Allocator a = vecn_make_allocator();

  float xs[] = { 1.0f, 2.0f, 3.0f, 4.0f };
  KL_VecN_f32 v = kl_vecn_f32_create(xs, 4, &a);
  KL_TEST_EXPECT_EQ_FLOAT(kl_vecn_f32_sum(v), 10.0f, VECN_EPS);

  float mixed[] = { 5.0f, -2.0f, -3.0f };
  KL_VecN_f32 w = kl_vecn_f32_create(mixed, 3, &a);
  KL_TEST_EXPECT_EQ_FLOAT(kl_vecn_f32_sum(w), 0.0f, VECN_EPS);

  kl_allocator_destroy(&a);
}

void test_vecn_f32_distance_sq() {
  KL_Allocator a = vecn_make_allocator();

  float xs[] = { 0.0f, 0.0f };
  float ys[] = { 3.0f, 4.0f };
  KL_VecN_f32 v = kl_vecn_f32_create(xs, 2, &a);
  KL_VecN_f32 w = kl_vecn_f32_create(ys, 2, &a);

  // 3^2 + 4^2 = 25 (no sqrt).
  KL_TEST_EXPECT_EQ_FLOAT(kl_vecn_f32_distance_sq(v, w), 25.0f, VECN_EPS);
  KL_TEST_EXPECT_EQ_FLOAT(kl_vecn_f32_distance_sq(w, w), 0.0f, VECN_EPS);
  // distance_sq is exactly distance squared.
  float d = kl_vecn_f32_distance(v, w);
  KL_TEST_EXPECT_EQ_FLOAT(kl_vecn_f32_distance_sq(v, w), d * d, VECN_EPS);
  // Symmetric.
  KL_TEST_EXPECT_EQ_FLOAT(kl_vecn_f32_distance_sq(v, w),
                          kl_vecn_f32_distance_sq(w, v), VECN_EPS);

  kl_allocator_destroy(&a);
}

void test_vecn_f32_manhattan_distance() {
  KL_Allocator a = vecn_make_allocator();

  float xs[] = { 1.0f, 2.0f };
  float ys[] = { 4.0f, 6.0f };
  KL_VecN_f32 v = kl_vecn_f32_create(xs, 2, &a);
  KL_VecN_f32 w = kl_vecn_f32_create(ys, 2, &a);

  // |4-1| + |6-2| = 3 + 4 = 7.
  KL_TEST_EXPECT_EQ_FLOAT(kl_vecn_f32_manhattan_distance(v, w), 7.0f, VECN_EPS);
  KL_TEST_EXPECT_EQ_FLOAT(kl_vecn_f32_manhattan_distance(w, w), 0.0f, VECN_EPS);
  // Symmetric (absolute differences).
  KL_TEST_EXPECT_EQ_FLOAT(kl_vecn_f32_manhattan_distance(v, w),
                          kl_vecn_f32_manhattan_distance(w, v), VECN_EPS);

  kl_allocator_destroy(&a);
}

void test_vecn_f32_equals() {
  KL_Allocator a = vecn_make_allocator();

  float xs[]   = { 1.0f, 2.0f, 3.0f };
  float same[] = { 1.0f, 2.0f, 3.0f };
  float near[] = { 1.0f, 2.0f, 3.0f + 1e-7f };  // within epsilon
  float far[]  = { 1.0f, 2.0f, 3.5f };          // beyond epsilon
  float short_[] = { 1.0f, 2.0f };

  KL_VecN_f32 v  = kl_vecn_f32_create(xs, 3, &a);
  KL_VecN_f32 vs = kl_vecn_f32_create(same, 3, &a);
  KL_VecN_f32 vn = kl_vecn_f32_create(near, 3, &a);
  KL_VecN_f32 vf = kl_vecn_f32_create(far, 3, &a);
  KL_VecN_f32 vshort = kl_vecn_f32_create(short_, 2, &a);

  KL_TEST_ASSERT(kl_vecn_f32_equals(v, vs, VECN_EPS), "identical vectors are equal");
  KL_TEST_ASSERT(kl_vecn_f32_equals(v, vn, VECN_EPS), "within-epsilon vectors are equal");
  KL_TEST_ASSERT(!kl_vecn_f32_equals(v, vf, VECN_EPS), "beyond-epsilon vectors are not equal");
  KL_TEST_ASSERT(!kl_vecn_f32_equals(v, vshort, VECN_EPS), "different-length vectors are not equal");

  kl_allocator_destroy(&a);
}

/* ===================== elementwise unary transforms ===================== */

void test_vecn_f32_negate() {
  KL_Allocator a = vecn_make_allocator();

  float xs[] = { 1.0f, -2.0f, 3.0f };
  KL_VecN_f32 v = kl_vecn_f32_create(xs, 3, &a);
  KL_VecN_f32 n = kl_vecn_f32_negate(v);

  float expected[] = { -1.0f, 2.0f, -3.0f };
  expect_items(n, expected, 3);
  KL_TEST_ASSERT(n.allocator == &a, "negate carries the source allocator");
  expect_items(v, xs, 3);  // source untouched

  // In-place variant.
  kl_vecn_f32_negate_ip(&v);
  expect_items(v, expected, 3);

  kl_allocator_destroy(&a);
}

void test_vecn_f32_abs() {
  KL_Allocator a = vecn_make_allocator();

  float xs[] = { -1.0f, 2.0f, -3.0f };
  KL_VecN_f32 v = kl_vecn_f32_create(xs, 3, &a);
  KL_VecN_f32 r = kl_vecn_f32_abs(v);

  float expected[] = { 1.0f, 2.0f, 3.0f };
  expect_items(r, expected, 3);
  KL_TEST_ASSERT(r.allocator == &a, "abs carries the source allocator");
  expect_items(v, xs, 3);

  kl_vecn_f32_abs_ip(&v);
  expect_items(v, expected, 3);

  kl_allocator_destroy(&a);
}

void test_vecn_f32_clamp() {
  KL_Allocator a = vecn_make_allocator();

  float xs[] = { -5.0f, 0.5f, 5.0f };
  KL_VecN_f32 v = kl_vecn_f32_create(xs, 3, &a);
  KL_VecN_f32 r = kl_vecn_f32_clamp(v, 0.0f, 1.0f);

  float expected[] = { 0.0f, 0.5f, 1.0f };
  expect_items(r, expected, 3);
  KL_TEST_ASSERT(r.allocator == &a, "clamp carries the source allocator");
  expect_items(v, xs, 3);

  kl_vecn_f32_clamp_ip(&v, 0.0f, 1.0f);
  expect_items(v, expected, 3);

  kl_allocator_destroy(&a);
}

void test_vecn_f32_lerp() {
  KL_Allocator a1 = vecn_make_allocator();
  KL_Allocator a2 = vecn_make_allocator();

  float xs[] = { 0.0f, 0.0f };
  float ys[] = { 10.0f, 20.0f };
  KL_VecN_f32 a = kl_vecn_f32_create(xs, 2, &a1);
  KL_VecN_f32 b = kl_vecn_f32_create(ys, 2, &a2);

  float at0[] = { 0.0f, 0.0f };
  float at1[] = { 10.0f, 20.0f };
  float athalf[] = { 5.0f, 10.0f };

  expect_items(kl_vecn_f32_lerp(a, b, 0.0f), at0, 2);
  expect_items(kl_vecn_f32_lerp(a, b, 1.0f), at1, 2);

  KL_VecN_f32 mid = kl_vecn_f32_lerp(a, b, 0.5f);
  expect_items(mid, athalf, 2);
  // Like the other binary ops, the result follows the left operand's allocator.
  KL_TEST_ASSERT(mid.allocator == &a1, "lerp result follows the left operand's allocator");
  // Operands untouched.
  expect_items(a, xs, 2);
  expect_items(b, ys, 2);

  // In-place variant.
  kl_vecn_f32_lerp_ip(&a, b, 0.5f);
  expect_items(a, athalf, 2);

  kl_allocator_destroy(&a1);
  kl_allocator_destroy(&a2);
}

/* ===================== SIMD path coverage =====================
 * The cases above use length 2-4, which never enter the 8-wide SIMD loop.
 * These exercise the SIMD body AND the scalar tail across the 8-lane boundary,
 * checking every op against an independent scalar reference. Values are small
 * integers so the reductions are exact regardless of summation order. */

static void check_vecn_ops_for_length(int64_t n) {
  KL_Allocator alloc = vecn_make_allocator();

  float abuf[64];
  float bbuf[64];
  for (int64_t i = 0; i < n; i++) {
    abuf[i] = (float)((i * 7 + 3) % 13) - 6.0f;  // integers in [-6, 6]
    bbuf[i] = (float)((i * 5 + 1) % 7) + 1.0f;   // integers in [1, 7], never 0
  }
  KL_VecN_f32 a = kl_vecn_f32_create(abuf, n, &alloc);
  KL_VecN_f32 b = kl_vecn_f32_create(bbuf, n, &alloc);

  // Reductions (exact for integer-valued floats).
  float sum_ref = 0.0f, magsq_ref = 0.0f, dot_ref = 0.0f, dsq_ref = 0.0f, manh_ref = 0.0f;
  for (int64_t i = 0; i < n; i++) {
    sum_ref   += abuf[i];
    magsq_ref += abuf[i] * abuf[i];
    dot_ref   += abuf[i] * bbuf[i];
    float d = bbuf[i] - abuf[i];
    dsq_ref  += d * d;
    manh_ref += fabsf(d);
  }
  KL_TEST_EXPECT_EQ_FLOAT(kl_vecn_f32_sum(a), sum_ref, VECN_EPS);
  KL_TEST_EXPECT_EQ_FLOAT(kl_vecn_f32_mag_sq(a), magsq_ref, VECN_EPS);
  KL_TEST_EXPECT_EQ_FLOAT(kl_vecn_f32_dot(a, b), dot_ref, VECN_EPS);
  KL_TEST_EXPECT_EQ_FLOAT(kl_vecn_f32_distance_sq(a, b), dsq_ref, VECN_EPS);
  KL_TEST_EXPECT_EQ_FLOAT(kl_vecn_f32_manhattan_distance(a, b), manh_ref, VECN_EPS);
  KL_TEST_EXPECT_EQ_FLOAT(kl_vecn_f32_mag(a), sqrtf(magsq_ref), VECN_EPS);
  KL_TEST_EXPECT_EQ_FLOAT(kl_vecn_f32_distance(a, b), sqrtf(dsq_ref), VECN_EPS);

  // Elementwise ops, each checked against a scalar reference.
  float exp_add[64], exp_sub[64], exp_mul[64], exp_div[64], exp_scale[64];
  float exp_neg[64], exp_abs[64], exp_clamp[64], exp_lerp[64];
  const float s = 2.5f, lo = -2.0f, hi = 3.0f, t = 0.25f;
  for (int64_t i = 0; i < n; i++) {
    exp_add[i]   = abuf[i] + bbuf[i];
    exp_sub[i]   = abuf[i] - bbuf[i];
    exp_mul[i]   = abuf[i] * bbuf[i];
    exp_div[i]   = abuf[i] / bbuf[i];
    exp_scale[i] = abuf[i] * s;
    exp_neg[i]   = -abuf[i];
    exp_abs[i]   = fabsf(abuf[i]);
    exp_clamp[i] = kl_clamp_f32(abuf[i], lo, hi);
    exp_lerp[i]  = kl_lerp_f32(abuf[i], bbuf[i], t);
  }
  expect_items(kl_vecn_f32_add(a, b), exp_add, n);
  expect_items(kl_vecn_f32_sub(a, b), exp_sub, n);
  expect_items(kl_vecn_f32_mul(a, b), exp_mul, n);
  expect_items(kl_vecn_f32_div(a, b), exp_div, n);
  expect_items(kl_vecn_f32_scale(a, s), exp_scale, n);
  expect_items(kl_vecn_f32_negate(a), exp_neg, n);
  expect_items(kl_vecn_f32_abs(a), exp_abs, n);
  expect_items(kl_vecn_f32_clamp(a, lo, hi), exp_clamp, n);
  expect_items(kl_vecn_f32_lerp(a, b, t), exp_lerp, n);

  // unit: result has magnitude 1 (vector is non-zero by construction).
  KL_TEST_EXPECT_EQ_FLOAT(kl_vecn_f32_mag(kl_vecn_f32_unit(a)), 1.0f, VECN_EPS);

  // equals across the SIMD boundary: a single differing element in the tail
  // (when n is not a multiple of 8) must still be detected.
  KL_TEST_ASSERT(kl_vecn_f32_equals(a, a, VECN_EPS), "vector equals itself");
  KL_VecN_f32 perturbed = kl_vecn_f32_copy(a);
  perturbed.items[n - 1] += 1.0f;
  KL_TEST_ASSERT(!kl_vecn_f32_equals(a, perturbed, VECN_EPS),
                 "a differing last element is detected");

  kl_allocator_destroy(&alloc);
}

void test_vecn_f32_simd_paths() {
  // scalar-only (<8), exact 8-lane blocks, and blocks + scalar tail.
  int64_t lengths[] = { 1, 7, 8, 9, 16, 17, 19, 32, 33 };
  for (int64_t i = 0; i < (int64_t)(sizeof(lengths) / sizeof(lengths[0])); i++) {
    check_vecn_ops_for_length(lengths[i]);
  }
}
