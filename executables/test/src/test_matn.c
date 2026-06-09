#include <test/tests.h>

/* ===================== matn (allocator-backed) test helpers ===================== */

#define MATN_EPS 1e-5f

// Like vecn, every allocating matn function must thread the source matrix's
// allocator through to its result. These tests pin that contract down, alongside
// the numeric behaviour of each operation.

static KL_Allocator matn_make_allocator(void) {
  KL_Allocator a = {0};
  kl_allocator_init(&a, (KL_AllocatorConfig){ .capacity = 8192 });
  return a;
}

static void expect_mat(KL_MatN_f32 m, int64_t rows, int64_t cols, const float* expected) {
  KL_TEST_EXPECT_EQ_INT(m.rows, rows);
  KL_TEST_EXPECT_EQ_INT(m.cols, cols);
  for (int64_t i = 0; i < rows * cols; i++) {
    KL_TEST_EXPECT_EQ_FLOAT(m.items[i], expected[i], MATN_EPS);
  }
}

/* ===================== create / copy / identity ===================== */

void test_matn_f32_create() {
  KL_Allocator a = matn_make_allocator();

  float xs[] = { 1.0f, 2.0f, 3.0f,
                 4.0f, 5.0f, 6.0f };
  KL_MatN_f32 m = kl_matn_f32_create(xs, 2, 3, &a);

  KL_TEST_ASSERT(m.items != 0, "create allocates a backing buffer");
  KL_TEST_ASSERT(m.items != xs, "create copies the source data, does not alias it");
  KL_TEST_ASSERT(m.allocator == &a, "create stores the allocator on the result");
  expect_mat(m, 2, 3, xs);

  kl_allocator_destroy(&a);
}

void test_matn_f32_create_null_items() {
  KL_Allocator a = matn_make_allocator();

  // A null `items` allocates without copying; the allocator zero-inits memory.
  KL_MatN_f32 m = kl_matn_f32_create(0, 2, 2, &a);

  KL_TEST_ASSERT(m.items != 0, "create still allocates when items is null");
  KL_TEST_ASSERT(m.allocator == &a, "create stores the allocator even with null items");
  float zeros[] = { 0.0f, 0.0f, 0.0f, 0.0f };
  expect_mat(m, 2, 2, zeros);

  kl_allocator_destroy(&a);
}

void test_matn_f32_copy() {
  KL_Allocator a = matn_make_allocator();

  float xs[] = { 1.0f, 2.0f,
                 3.0f, 4.0f };
  KL_MatN_f32 src = kl_matn_f32_create(xs, 2, 2, &a);
  KL_MatN_f32 dst = kl_matn_f32_copy(src);

  KL_TEST_ASSERT(dst.items != src.items, "copy produces an independent buffer");
  KL_TEST_ASSERT(dst.allocator == &a, "copy carries the source allocator");
  expect_mat(dst, 2, 2, xs);

  // Mutating the copy must not touch the source.
  dst.items[0] = 99.0f;
  KL_TEST_EXPECT_EQ_FLOAT(src.items[0], 1.0f, MATN_EPS);

  kl_allocator_destroy(&a);
}

void test_matn_f32_identity() {
  KL_Allocator a = matn_make_allocator();

  KL_MatN_f32 m = kl_matn_f32_identity(3, &a);

  float expected[] = { 1.0f, 0.0f, 0.0f,
                       0.0f, 1.0f, 0.0f,
                       0.0f, 0.0f, 1.0f };
  expect_mat(m, 3, 3, expected);
  KL_TEST_ASSERT(m.allocator == &a, "identity stores the allocator");

  kl_allocator_destroy(&a);
}

/* ===================== element access ===================== */

void test_matn_f32_at_set() {
  KL_Allocator a = matn_make_allocator();

  float xs[] = { 1.0f, 2.0f, 3.0f,
                 4.0f, 5.0f, 6.0f };
  KL_MatN_f32 m = kl_matn_f32_create(xs, 2, 3, &a);

  // Row-major: at(r, c) == items[r * cols + c].
  KL_TEST_EXPECT_EQ_FLOAT(kl_matn_f32_at(m, 0, 0), 1.0f, MATN_EPS);
  KL_TEST_EXPECT_EQ_FLOAT(kl_matn_f32_at(m, 0, 2), 3.0f, MATN_EPS);
  KL_TEST_EXPECT_EQ_FLOAT(kl_matn_f32_at(m, 1, 0), 4.0f, MATN_EPS);
  KL_TEST_EXPECT_EQ_FLOAT(kl_matn_f32_at(m, 1, 2), 6.0f, MATN_EPS);

  // set writes through the shared backing store and is visible via at.
  kl_matn_f32_set(m, 1, 1, 42.0f);
  KL_TEST_EXPECT_EQ_FLOAT(kl_matn_f32_at(m, 1, 1), 42.0f, MATN_EPS);
  KL_TEST_EXPECT_EQ_FLOAT(m.items[1 * 3 + 1], 42.0f, MATN_EPS);

  kl_allocator_destroy(&a);
}

/* ===================== rows ===================== */

void test_matn_f32_get_row() {
  KL_Allocator a = matn_make_allocator();

  float xs[] = { 1.0f, 2.0f, 3.0f,
                 4.0f, 5.0f, 6.0f };
  KL_MatN_f32 m = kl_matn_f32_create(xs, 2, 3, &a);

  KL_VecN_f32 row1 = kl_matn_f32_get_row(m, 1);

  // get_row is a non-owning view: length == cols, items alias the matrix row.
  KL_TEST_EXPECT_EQ_INT(row1.length, 3);
  KL_TEST_ASSERT(row1.items == &m.items[1 * 3], "get_row aliases the matrix backing store");
  KL_TEST_ASSERT(row1.allocator == &a, "get_row carries the matrix allocator");

  // Writing through the view mutates the matrix (it is a view, not a copy).
  row1.items[0] = 99.0f;
  KL_TEST_EXPECT_EQ_FLOAT(kl_matn_f32_at(m, 1, 0), 99.0f, MATN_EPS);

  kl_allocator_destroy(&a);
}

void test_matn_f32_copy_row() {
  KL_Allocator a = matn_make_allocator();

  float xs[] = { 1.0f, 2.0f, 3.0f,
                 4.0f, 5.0f, 6.0f };
  KL_MatN_f32 m = kl_matn_f32_create(xs, 2, 3, &a);

  KL_VecN_f32 row0 = kl_matn_f32_copy_row(m, 0);

  float expected[] = { 1.0f, 2.0f, 3.0f };
  KL_TEST_EXPECT_EQ_INT(row0.length, 3);
  KL_TEST_ASSERT(row0.items != &m.items[0], "copy_row produces an independent buffer");
  for (int64_t i = 0; i < 3; i++) {
    KL_TEST_EXPECT_EQ_FLOAT(row0.items[i], expected[i], MATN_EPS);
  }

  // Mutating the copy must not touch the matrix.
  row0.items[0] = 99.0f;
  KL_TEST_EXPECT_EQ_FLOAT(kl_matn_f32_at(m, 0, 0), 1.0f, MATN_EPS);

  kl_allocator_destroy(&a);
}

/* ===================== equals ===================== */

void test_matn_f32_equals() {
  KL_Allocator a = matn_make_allocator();

  float xs[]   = { 1.0f, 2.0f, 3.0f, 4.0f };
  float same[] = { 1.0f, 2.0f, 3.0f, 4.0f };
  float near[] = { 1.0f, 2.0f, 3.0f, 4.0f + 1e-7f };  // within epsilon
  float far[]  = { 1.0f, 2.0f, 3.0f, 4.5f };          // beyond epsilon

  KL_MatN_f32 m   = kl_matn_f32_create(xs, 2, 2, &a);
  KL_MatN_f32 ms  = kl_matn_f32_create(same, 2, 2, &a);
  KL_MatN_f32 mn  = kl_matn_f32_create(near, 2, 2, &a);
  KL_MatN_f32 mf  = kl_matn_f32_create(far, 2, 2, &a);
  // Same element count, different shape (2x2 vs 4x1).
  KL_MatN_f32 mshape = kl_matn_f32_create(xs, 4, 1, &a);

  KL_TEST_ASSERT(kl_matn_f32_equals(m, ms, MATN_EPS), "identical matrices are equal");
  KL_TEST_ASSERT(kl_matn_f32_equals(m, mn, MATN_EPS), "within-epsilon matrices are equal");
  KL_TEST_ASSERT(!kl_matn_f32_equals(m, mf, MATN_EPS), "beyond-epsilon matrices are not equal");
  KL_TEST_ASSERT(!kl_matn_f32_equals(m, mshape, MATN_EPS),
                 "same element count but different shape is not equal");

  kl_allocator_destroy(&a);
}

/* ===================== transpose ===================== */

void test_matn_f32_transpose() {
  KL_Allocator a = matn_make_allocator();

  float xs[] = { 1.0f, 2.0f, 3.0f,
                 4.0f, 5.0f, 6.0f };
  KL_MatN_f32 m = kl_matn_f32_create(xs, 2, 3, &a);

  KL_MatN_f32 t = kl_matn_f32_transpose(m);

  // 2x3 -> 3x2, with t[c][r] == m[r][c].
  float expected[] = { 1.0f, 4.0f,
                       2.0f, 5.0f,
                       3.0f, 6.0f };
  expect_mat(t, 3, 2, expected);
  KL_TEST_ASSERT(t.allocator == &a, "transpose carries the source allocator");
  // Source untouched.
  expect_mat(m, 2, 3, xs);

  // Transposing twice returns the original.
  KL_MatN_f32 tt = kl_matn_f32_transpose(t);
  KL_TEST_ASSERT(kl_matn_f32_equals(tt, m, MATN_EPS), "transpose is an involution");

  kl_allocator_destroy(&a);
}

/* ===================== matrix multiplication ===================== */

void test_matn_f32_mul() {
  KL_Allocator a1 = matn_make_allocator();
  KL_Allocator a2 = matn_make_allocator();

  float xs[] = { 1.0f, 2.0f, 3.0f,
                 4.0f, 5.0f, 6.0f };          // 2x3
  float ys[] = { 7.0f,  8.0f,
                 9.0f,  10.0f,
                 11.0f, 12.0f };              // 3x2
  KL_MatN_f32 a = kl_matn_f32_create(xs, 2, 3, &a1);
  KL_MatN_f32 b = kl_matn_f32_create(ys, 3, 2, &a2);

  KL_MatN_f32 r = kl_matn_f32_mul(a, b);

  // [1 2 3; 4 5 6] * [7 8; 9 10; 11 12] = [58 64; 139 154].
  float expected[] = { 58.0f, 64.0f,
                       139.0f, 154.0f };
  expect_mat(r, 2, 2, expected);
  KL_TEST_ASSERT(r.allocator == &a1, "mul result follows the left operand's allocator");
  // Operands untouched.
  expect_mat(a, 2, 3, xs);
  expect_mat(b, 3, 2, ys);

  // Multiplying by the identity is a no-op (both sides).
  KL_MatN_f32 i2 = kl_matn_f32_identity(2, &a1);
  KL_MatN_f32 i3 = kl_matn_f32_identity(3, &a1);
  KL_TEST_ASSERT(kl_matn_f32_equals(kl_matn_f32_mul(i2, a), a, MATN_EPS), "I * A == A");
  KL_TEST_ASSERT(kl_matn_f32_equals(kl_matn_f32_mul(a, i3), a, MATN_EPS), "A * I == A");

  kl_allocator_destroy(&a1);
  kl_allocator_destroy(&a2);
}

void test_matn_f32_mul_into() {
  KL_Allocator a = matn_make_allocator();

  float xs[] = { 1.0f, 2.0f, 3.0f,
                 4.0f, 5.0f, 6.0f };          // 2x3
  float ys[] = { 7.0f,  8.0f,
                 9.0f,  10.0f,
                 11.0f, 12.0f };              // 3x2
  KL_MatN_f32 m = kl_matn_f32_create(xs, 2, 3, &a);
  KL_MatN_f32 b = kl_matn_f32_create(ys, 3, 2, &a);

  // Caller owns the destination; mul_into must not allocate, only fill it.
  KL_MatN_f32 out = kl_matn_f32_create(0, 2, 2, &a);
  float* out_buf = out.items;
  kl_matn_f32_mul_into(m, b, &out);

  float expected[] = { 58.0f, 64.0f,
                       139.0f, 154.0f };
  expect_mat(out, 2, 2, expected);
  KL_TEST_ASSERT(out.items == out_buf, "mul_into writes into the caller's buffer, no realloc");

  // Result matches the allocating variant exactly.
  KL_MatN_f32 ref = kl_matn_f32_mul(m, b);
  KL_TEST_ASSERT(kl_matn_f32_equals(out, ref, MATN_EPS), "mul_into matches kl_matn_f32_mul");

  // A pre-filled destination is overwritten, not accumulated into.
  kl_matn_f32_set(out, 0, 0, 999.0f);
  kl_matn_f32_mul_into(m, b, &out);
  expect_mat(out, 2, 2, expected);

  kl_allocator_destroy(&a);
}

/* ===================== matrix-vector multiplication ===================== */

void test_matn_f32_mul_vec() {
  KL_Allocator a1 = matn_make_allocator();
  KL_Allocator a2 = matn_make_allocator();

  float xs[] = { 1.0f, 2.0f, 3.0f,
                 4.0f, 5.0f, 6.0f };          // 2x3
  float vs[] = { 1.0f, 0.0f, -1.0f };          // length 3
  KL_MatN_f32 m = kl_matn_f32_create(xs, 2, 3, &a1);
  KL_VecN_f32 v = kl_vecn_f32_create(vs, 3, &a2);

  KL_VecN_f32 r = kl_matn_f32_mul_vec(m, v);

  // row0 . v = 1-3 = -2; row1 . v = 4-6 = -2.
  KL_TEST_EXPECT_EQ_INT(r.length, 2);
  KL_TEST_EXPECT_EQ_FLOAT(r.items[0], -2.0f, MATN_EPS);
  KL_TEST_EXPECT_EQ_FLOAT(r.items[1], -2.0f, MATN_EPS);
  KL_TEST_ASSERT(r.allocator == &a1, "mul_vec result follows the matrix allocator");

  // Identity matrix maps a vector to itself.
  KL_MatN_f32 i3 = kl_matn_f32_identity(3, &a1);
  KL_VecN_f32 same = kl_matn_f32_mul_vec(i3, v);
  KL_TEST_ASSERT(kl_vecn_f32_equals(same, v, MATN_EPS), "I * v == v");

  kl_allocator_destroy(&a1);
  kl_allocator_destroy(&a2);
}

/* ===================== elementwise scalar / unary ops ===================== */

void test_matn_f32_scale() {
  KL_Allocator a = matn_make_allocator();

  float xs[] = { 1.0f, -2.0f,
                 3.0f, -4.0f };
  KL_MatN_f32 m = kl_matn_f32_create(xs, 2, 2, &a);
  KL_MatN_f32 r = kl_matn_f32_scale(m, 2.0f);

  float expected[] = { 2.0f, -4.0f, 6.0f, -8.0f };
  expect_mat(r, 2, 2, expected);
  KL_TEST_ASSERT(r.allocator == &a, "scale carries the source allocator");
  expect_mat(m, 2, 2, xs);  // source untouched

  // In-place variant.
  kl_matn_f32_scale_ip(&m, 2.0f);
  expect_mat(m, 2, 2, expected);

  kl_allocator_destroy(&a);
}

void test_matn_f32_negate() {
  KL_Allocator a = matn_make_allocator();

  float xs[] = { 1.0f, -2.0f, 3.0f, -4.0f };
  KL_MatN_f32 m = kl_matn_f32_create(xs, 2, 2, &a);
  KL_MatN_f32 r = kl_matn_f32_negate(m);

  float expected[] = { -1.0f, 2.0f, -3.0f, 4.0f };
  expect_mat(r, 2, 2, expected);
  KL_TEST_ASSERT(r.allocator == &a, "negate carries the source allocator");
  expect_mat(m, 2, 2, xs);

  kl_matn_f32_negate_ip(&m);
  expect_mat(m, 2, 2, expected);

  kl_allocator_destroy(&a);
}

void test_matn_f32_abs() {
  KL_Allocator a = matn_make_allocator();

  float xs[] = { -1.0f, 2.0f, -3.0f, 4.0f };
  KL_MatN_f32 m = kl_matn_f32_create(xs, 2, 2, &a);
  KL_MatN_f32 r = kl_matn_f32_abs(m);

  float expected[] = { 1.0f, 2.0f, 3.0f, 4.0f };
  expect_mat(r, 2, 2, expected);
  KL_TEST_ASSERT(r.allocator == &a, "abs carries the source allocator");
  expect_mat(m, 2, 2, xs);

  kl_matn_f32_abs_ip(&m);
  expect_mat(m, 2, 2, expected);

  kl_allocator_destroy(&a);
}

void test_matn_f32_clamp() {
  KL_Allocator a = matn_make_allocator();

  float xs[] = { -5.0f, 0.5f, 5.0f, 1.0f };
  KL_MatN_f32 m = kl_matn_f32_create(xs, 2, 2, &a);
  KL_MatN_f32 r = kl_matn_f32_clamp(m, 0.0f, 1.0f);

  float expected[] = { 0.0f, 0.5f, 1.0f, 1.0f };
  expect_mat(r, 2, 2, expected);
  KL_TEST_ASSERT(r.allocator == &a, "clamp carries the source allocator");
  expect_mat(m, 2, 2, xs);

  kl_matn_f32_clamp_ip(&m, 0.0f, 1.0f);
  expect_mat(m, 2, 2, expected);

  kl_allocator_destroy(&a);
}

void test_matn_f32_lerp() {
  KL_Allocator a1 = matn_make_allocator();
  KL_Allocator a2 = matn_make_allocator();

  float xs[] = { 0.0f, 0.0f, 0.0f, 0.0f };
  float ys[] = { 10.0f, 20.0f, 30.0f, 40.0f };
  KL_MatN_f32 a = kl_matn_f32_create(xs, 2, 2, &a1);
  KL_MatN_f32 b = kl_matn_f32_create(ys, 2, 2, &a2);

  float athalf[] = { 5.0f, 10.0f, 15.0f, 20.0f };

  expect_mat(kl_matn_f32_lerp(a, b, 0.0f), 2, 2, xs);
  expect_mat(kl_matn_f32_lerp(a, b, 1.0f), 2, 2, ys);

  KL_MatN_f32 mid = kl_matn_f32_lerp(a, b, 0.5f);
  expect_mat(mid, 2, 2, athalf);
  KL_TEST_ASSERT(mid.allocator == &a1, "lerp result follows the left operand's allocator");
  // Operands untouched.
  expect_mat(a, 2, 2, xs);
  expect_mat(b, 2, 2, ys);

  // In-place variant.
  kl_matn_f32_lerp_ip(&a, b, 0.5f);
  expect_mat(a, 2, 2, athalf);

  kl_allocator_destroy(&a1);
  kl_allocator_destroy(&a2);
}

/* ===================== elementwise matrix-matrix ops ===================== */

// These build their result from the LEFT operand, so the result must carry
// `a`'s allocator regardless of where `b` came from.

void test_matn_f32_add() {
  KL_Allocator a1 = matn_make_allocator();
  KL_Allocator a2 = matn_make_allocator();

  float xs[] = { 1.0f, 2.0f, 3.0f, 4.0f };
  float ys[] = { 5.0f, 6.0f, 7.0f, 8.0f };
  KL_MatN_f32 a = kl_matn_f32_create(xs, 2, 2, &a1);
  KL_MatN_f32 b = kl_matn_f32_create(ys, 2, 2, &a2);

  KL_MatN_f32 r = kl_matn_f32_add(a, b);

  float expected[] = { 6.0f, 8.0f, 10.0f, 12.0f };
  expect_mat(r, 2, 2, expected);
  KL_TEST_ASSERT(r.allocator == &a1, "add result follows the left operand's allocator");
  expect_mat(a, 2, 2, xs);
  expect_mat(b, 2, 2, ys);

  kl_matn_f32_add_ip(&a, b);
  expect_mat(a, 2, 2, expected);

  kl_allocator_destroy(&a1);
  kl_allocator_destroy(&a2);
}

void test_matn_f32_sub() {
  KL_Allocator a1 = matn_make_allocator();
  KL_Allocator a2 = matn_make_allocator();

  float xs[] = { 5.0f, 7.0f, 9.0f, 11.0f };
  float ys[] = { 1.0f, 2.0f, 3.0f, 4.0f };
  KL_MatN_f32 a = kl_matn_f32_create(xs, 2, 2, &a1);
  KL_MatN_f32 b = kl_matn_f32_create(ys, 2, 2, &a2);

  KL_MatN_f32 r = kl_matn_f32_sub(a, b);

  float expected[] = { 4.0f, 5.0f, 6.0f, 7.0f };
  expect_mat(r, 2, 2, expected);
  KL_TEST_ASSERT(r.allocator == &a1, "sub result follows the left operand's allocator");
  expect_mat(a, 2, 2, xs);
  expect_mat(b, 2, 2, ys);

  kl_matn_f32_sub_ip(&a, b);
  expect_mat(a, 2, 2, expected);

  kl_allocator_destroy(&a1);
  kl_allocator_destroy(&a2);
}

void test_matn_f32_mul_elementwise() {
  KL_Allocator a1 = matn_make_allocator();
  KL_Allocator a2 = matn_make_allocator();

  float xs[] = { 2.0f, 3.0f, 4.0f, 5.0f };
  float ys[] = { 5.0f, 6.0f, 7.0f, 8.0f };
  KL_MatN_f32 a = kl_matn_f32_create(xs, 2, 2, &a1);
  KL_MatN_f32 b = kl_matn_f32_create(ys, 2, 2, &a2);

  KL_MatN_f32 r = kl_matn_f32_mul_elementwise(a, b);

  // Hadamard product: elementwise, NOT matrix multiplication.
  float expected[] = { 10.0f, 18.0f, 28.0f, 40.0f };
  expect_mat(r, 2, 2, expected);
  KL_TEST_ASSERT(r.allocator == &a1, "mul_elementwise result follows the left operand's allocator");
  expect_mat(a, 2, 2, xs);
  expect_mat(b, 2, 2, ys);

  kl_matn_f32_mul_elementwise_ip(&a, b);
  expect_mat(a, 2, 2, expected);

  kl_allocator_destroy(&a1);
  kl_allocator_destroy(&a2);
}

void test_matn_f32_div() {
  KL_Allocator a1 = matn_make_allocator();
  KL_Allocator a2 = matn_make_allocator();

  float xs[] = { 10.0f, 18.0f, 28.0f, 40.0f };
  float ys[] = { 5.0f, 6.0f, 7.0f, 8.0f };
  KL_MatN_f32 a = kl_matn_f32_create(xs, 2, 2, &a1);
  KL_MatN_f32 b = kl_matn_f32_create(ys, 2, 2, &a2);

  KL_MatN_f32 r = kl_matn_f32_div(a, b);

  float expected[] = { 2.0f, 3.0f, 4.0f, 5.0f };
  expect_mat(r, 2, 2, expected);
  KL_TEST_ASSERT(r.allocator == &a1, "div result follows the left operand's allocator");
  expect_mat(a, 2, 2, xs);
  expect_mat(b, 2, 2, ys);

  kl_matn_f32_div_ip(&a, b);
  expect_mat(a, 2, 2, expected);

  kl_allocator_destroy(&a1);
  kl_allocator_destroy(&a2);
}

// The four elementwise binops all gained an _into form from the same macro;
// check each matches its allocating counterpart, reuses the caller's buffer,
// leaves operands alone, and tolerates out aliasing the left operand.
void test_matn_f32_binop_into() {
  KL_Allocator a = matn_make_allocator();

  float xs[] = { 10.0f, 18.0f, 28.0f, 40.0f };
  float ys[] = { 5.0f, 6.0f, 7.0f, 8.0f };
  KL_MatN_f32 m = kl_matn_f32_create(xs, 2, 2, &a);
  KL_MatN_f32 n = kl_matn_f32_create(ys, 2, 2, &a);

  KL_MatN_f32 out = kl_matn_f32_create(0, 2, 2, &a);
  float* buf = out.items;

  kl_matn_f32_add_into(m, n, &out);
  KL_TEST_ASSERT(out.items == buf, "add_into reuses the caller buffer, no realloc");
  KL_TEST_ASSERT(kl_matn_f32_equals(out, kl_matn_f32_add(m, n), MATN_EPS), "add_into matches add");

  kl_matn_f32_sub_into(m, n, &out);
  KL_TEST_ASSERT(kl_matn_f32_equals(out, kl_matn_f32_sub(m, n), MATN_EPS), "sub_into matches sub");

  kl_matn_f32_mul_elementwise_into(m, n, &out);
  KL_TEST_ASSERT(kl_matn_f32_equals(out, kl_matn_f32_mul_elementwise(m, n), MATN_EPS),
                 "mul_elementwise_into matches mul_elementwise");

  kl_matn_f32_div_into(m, n, &out);
  KL_TEST_ASSERT(kl_matn_f32_equals(out, kl_matn_f32_div(m, n), MATN_EPS), "div_into matches div");

  // Operands are never mutated.
  expect_mat(m, 2, 2, xs);
  expect_mat(n, 2, 2, ys);

  // out aliasing the left operand is allowed and equals the _ip / allocating result.
  KL_MatN_f32 acopy = kl_matn_f32_copy(m);
  kl_matn_f32_add_into(acopy, n, &acopy);
  KL_TEST_ASSERT(kl_matn_f32_equals(acopy, kl_matn_f32_add(m, n), MATN_EPS),
                 "add_into with out == a degenerates to in-place");

  kl_allocator_destroy(&a);
}

// The unary transforms (scale/negate/abs/clamp) and lerp each gained an _into
// form too; check they match their allocating counterparts and reuse the buffer.
void test_matn_f32_unary_into() {
  KL_Allocator a = matn_make_allocator();

  float xs[] = { -5.0f, 0.5f, 5.0f, -1.0f };
  float ys[] = { 10.0f, 20.0f, 30.0f, 40.0f };
  KL_MatN_f32 m = kl_matn_f32_create(xs, 2, 2, &a);
  KL_MatN_f32 n = kl_matn_f32_create(ys, 2, 2, &a);

  KL_MatN_f32 out = kl_matn_f32_create(0, 2, 2, &a);
  float* buf = out.items;

  kl_matn_f32_scale_into(m, 2.0f, &out);
  KL_TEST_ASSERT(out.items == buf, "scale_into reuses the caller buffer, no realloc");
  KL_TEST_ASSERT(kl_matn_f32_equals(out, kl_matn_f32_scale(m, 2.0f), MATN_EPS), "scale_into matches scale");

  kl_matn_f32_negate_into(m, &out);
  KL_TEST_ASSERT(kl_matn_f32_equals(out, kl_matn_f32_negate(m), MATN_EPS), "negate_into matches negate");

  kl_matn_f32_abs_into(m, &out);
  KL_TEST_ASSERT(kl_matn_f32_equals(out, kl_matn_f32_abs(m), MATN_EPS), "abs_into matches abs");

  kl_matn_f32_clamp_into(m, 0.0f, 1.0f, &out);
  KL_TEST_ASSERT(kl_matn_f32_equals(out, kl_matn_f32_clamp(m, 0.0f, 1.0f), MATN_EPS), "clamp_into matches clamp");

  kl_matn_f32_lerp_into(m, n, 0.25f, &out);
  KL_TEST_ASSERT(kl_matn_f32_equals(out, kl_matn_f32_lerp(m, n, 0.25f), MATN_EPS), "lerp_into matches lerp");

  // Operands untouched throughout.
  expect_mat(m, 2, 2, xs);
  expect_mat(n, 2, 2, ys);

  // Unary _into may alias its operand (no second operand to clobber).
  KL_MatN_f32 acopy = kl_matn_f32_copy(m);
  kl_matn_f32_negate_into(acopy, &acopy);
  KL_TEST_ASSERT(kl_matn_f32_equals(acopy, kl_matn_f32_negate(m), MATN_EPS),
                 "negate_into with out == a degenerates to in-place");

  kl_allocator_destroy(&a);
}

/* ===================== SIMD path coverage =====================
 * The cases above are small. These exercise the vectorized inner loops AND the
 * scalar tails across the 8-lane boundary, checking against independent scalar
 * references. Values are small integers so results are exact regardless of
 * summation/accumulation order (matmul's ikj order reorders the sums). */

// matmul: the inner j loop runs over b.cols, so vary b.cols around the boundary.
static void check_matn_mul_for_dims(int64_t m, int64_t k, int64_t n) {
  KL_Allocator alloc = matn_make_allocator();

  float abuf[256];
  float bbuf[256];
  for (int64_t i = 0; i < m * k; i++) abuf[i] = (float)((i * 7 + 3) % 11) - 5.0f;  // [-5, 5]
  for (int64_t i = 0; i < k * n; i++) bbuf[i] = (float)((i * 5 + 1) % 7) - 3.0f;   // [-3, 3]

  KL_MatN_f32 a = kl_matn_f32_create(abuf, m, k, &alloc);
  KL_MatN_f32 b = kl_matn_f32_create(bbuf, k, n, &alloc);
  KL_MatN_f32 r = kl_matn_f32_mul(a, b);

  KL_TEST_EXPECT_EQ_INT(r.rows, m);
  KL_TEST_EXPECT_EQ_INT(r.cols, n);
  for (int64_t i = 0; i < m; i++) {
    for (int64_t j = 0; j < n; j++) {
      float ref = 0.0f;
      for (int64_t p = 0; p < k; p++) ref += abuf[i * k + p] * bbuf[p * n + j];
      KL_TEST_EXPECT_EQ_FLOAT(r.items[i * n + j], ref, MATN_EPS);
    }
  }

  kl_allocator_destroy(&alloc);
}

// Elementwise ops run over the flat rows*cols buffer; vary the total size.
static void check_matn_elementwise_for_size(int64_t rows, int64_t cols) {
  KL_Allocator alloc = matn_make_allocator();
  int64_t total = rows * cols;

  float abuf[256];
  float bbuf[256];
  for (int64_t i = 0; i < total; i++) {
    abuf[i] = (float)((i * 7 + 3) % 13) - 6.0f;  // [-6, 6]
    bbuf[i] = (float)((i * 5 + 1) % 7) + 1.0f;   // [1, 7], never 0
  }
  KL_MatN_f32 a = kl_matn_f32_create(abuf, rows, cols, &alloc);
  KL_MatN_f32 b = kl_matn_f32_create(bbuf, rows, cols, &alloc);

  float exp_add[256], exp_sub[256], exp_mul[256], exp_div[256], exp_scale[256];
  float exp_neg[256], exp_abs[256], exp_clamp[256], exp_lerp[256];
  const float s = 2.5f, lo = -2.0f, hi = 3.0f, t = 0.25f;
  for (int64_t i = 0; i < total; i++) {
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
  expect_mat(kl_matn_f32_add(a, b), rows, cols, exp_add);
  expect_mat(kl_matn_f32_sub(a, b), rows, cols, exp_sub);
  expect_mat(kl_matn_f32_mul_elementwise(a, b), rows, cols, exp_mul);
  expect_mat(kl_matn_f32_div(a, b), rows, cols, exp_div);
  expect_mat(kl_matn_f32_scale(a, s), rows, cols, exp_scale);
  expect_mat(kl_matn_f32_negate(a), rows, cols, exp_neg);
  expect_mat(kl_matn_f32_abs(a), rows, cols, exp_abs);
  expect_mat(kl_matn_f32_clamp(a, lo, hi), rows, cols, exp_clamp);
  expect_mat(kl_matn_f32_lerp(a, b, t), rows, cols, exp_lerp);

  // mul_vec also rides vecn's SIMD dot over each row; check against a reference.
  KL_VecN_f32 v = kl_vecn_f32_create(bbuf, cols, &alloc);  // length == cols
  KL_VecN_f32 mv = kl_matn_f32_mul_vec(a, v);
  KL_TEST_EXPECT_EQ_INT(mv.length, rows);
  for (int64_t i = 0; i < rows; i++) {
    float ref = 0.0f;
    for (int64_t j = 0; j < cols; j++) ref += abuf[i * cols + j] * bbuf[j];
    KL_TEST_EXPECT_EQ_FLOAT(mv.items[i], ref, MATN_EPS);
  }

  kl_allocator_destroy(&alloc);
}

void test_matn_f32_simd_paths() {
  // matmul: inner loop over n spans scalar-only, exact blocks, and block+tail.
  int64_t dims[][3] = {
    {1, 1, 1}, {2, 3, 5}, {3, 5, 8}, {2, 8, 9}, {4, 4, 16}, {3, 7, 17}, {5, 9, 19},
  };
  for (int64_t i = 0; i < (int64_t)(sizeof(dims) / sizeof(dims[0])); i++) {
    check_matn_mul_for_dims(dims[i][0], dims[i][1], dims[i][2]);
  }

  // Elementwise: total buffer size spanning the 8-lane boundary.
  int64_t shapes[][2] = {
    {1, 7}, {1, 8}, {1, 9}, {2, 8}, {3, 5}, {4, 4}, {3, 7}, {5, 7},
  };
  for (int64_t i = 0; i < (int64_t)(sizeof(shapes) / sizeof(shapes[0])); i++) {
    check_matn_elementwise_for_size(shapes[i][0], shapes[i][1]);
  }
}
