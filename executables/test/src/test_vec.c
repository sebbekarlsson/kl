#include <test/tests.h>

/* ===================== vector test helpers ===================== */

#define VEC_EPS 1e-5f

#define EXPECT_VEC2(V, X, Y) do { \
  KL_Vec2 _v = (V); \
  KL_TEST_EXPECT_EQ_FLOAT(_v.x, (X), VEC_EPS); \
  KL_TEST_EXPECT_EQ_FLOAT(_v.y, (Y), VEC_EPS); \
} while (0)

#define EXPECT_VEC3(V, X, Y, Z) do { \
  KL_Vec3 _v = (V); \
  KL_TEST_EXPECT_EQ_FLOAT(_v.x, (X), VEC_EPS); \
  KL_TEST_EXPECT_EQ_FLOAT(_v.y, (Y), VEC_EPS); \
  KL_TEST_EXPECT_EQ_FLOAT(_v.z, (Z), VEC_EPS); \
} while (0)

#define EXPECT_VEC4(V, X, Y, Z, W) do { \
  KL_Vec4 _v = (V); \
  KL_TEST_EXPECT_EQ_FLOAT(_v.x, (X), VEC_EPS); \
  KL_TEST_EXPECT_EQ_FLOAT(_v.y, (Y), VEC_EPS); \
  KL_TEST_EXPECT_EQ_FLOAT(_v.z, (Z), VEC_EPS); \
  KL_TEST_EXPECT_EQ_FLOAT(_v.w, (W), VEC_EPS); \
} while (0)

/* ===================== vec2 tests ===================== */

void test_vec2_constructors() {
  // The union aliases items[] over the named x/y fields.
  EXPECT_VEC2(kl_vec2(1.0f, 2.0f), 1.0f, 2.0f);
  EXPECT_VEC2(kl_vec21(5.0f), 5.0f, 5.0f);

  KL_Vec2 a = kl_vec2(3.0f, 4.0f);
  KL_TEST_EXPECT_EQ_FLOAT(a.items[0], 3.0f, VEC_EPS);
  KL_TEST_EXPECT_EQ_FLOAT(a.items[1], 4.0f, VEC_EPS);
}

void test_vec2_mag() {
  KL_TEST_EXPECT_EQ_FLOAT(kl_vec2_mag(kl_vec2(3.0f, 4.0f)), 5.0f, VEC_EPS);
  KL_TEST_EXPECT_EQ_FLOAT(kl_vec2_mag(kl_vec21(0.0f)), 0.0f, VEC_EPS);
  KL_TEST_EXPECT_EQ_FLOAT(kl_vec2_mag(kl_vec2(-3.0f, -4.0f)), 5.0f, VEC_EPS);
}

void test_vec2_dot() {
  KL_TEST_EXPECT_EQ_FLOAT(kl_vec2_dot(kl_vec2(1.0f, 2.0f), kl_vec2(3.0f, 4.0f)), 11.0f, VEC_EPS);
  // Orthogonal vectors have a zero dot product.
  KL_TEST_EXPECT_EQ_FLOAT(kl_vec2_dot(kl_vec2(1.0f, 0.0f), kl_vec2(0.0f, 1.0f)), 0.0f, VEC_EPS);
  // Dot with self equals magnitude squared.
  KL_Vec2 a = kl_vec2(3.0f, 4.0f);
  KL_TEST_EXPECT_EQ_FLOAT(kl_vec2_dot(a, a), 25.0f, VEC_EPS);
}

void test_vec2_unit() {
  KL_Vec2 u = kl_vec2_unit(kl_vec2(3.0f, 4.0f));
  EXPECT_VEC2(u, 0.6f, 0.8f);
  KL_TEST_EXPECT_EQ_FLOAT(kl_vec2_mag(u), 1.0f, VEC_EPS);

  // A zero vector has no direction; unit leaves it zeroed rather than dividing by zero.
  EXPECT_VEC2(kl_vec2_unit(kl_vec21(0.0f)), 0.0f, 0.0f);
}

void test_vec2_scale() {
  EXPECT_VEC2(kl_vec2_scale(kl_vec2(1.0f, -2.0f), 3.0f), 3.0f, -6.0f);
  EXPECT_VEC2(kl_vec2_scale(kl_vec2(5.0f, 7.0f), 0.0f), 0.0f, 0.0f);
}

void test_vec2_add() {
  EXPECT_VEC2(kl_vec2_add(kl_vec2(1.0f, 2.0f), kl_vec2(3.0f, 4.0f)), 4.0f, 6.0f);
  EXPECT_VEC2(kl_vec2_add(kl_vec2(1.0f, 2.0f), kl_vec2(-1.0f, -2.0f)), 0.0f, 0.0f);
}

void test_vec2_sub() {
  EXPECT_VEC2(kl_vec2_sub(kl_vec2(5.0f, 7.0f), kl_vec2(2.0f, 3.0f)), 3.0f, 4.0f);
  EXPECT_VEC2(kl_vec2_sub(kl_vec2(1.0f, 1.0f), kl_vec2(1.0f, 1.0f)), 0.0f, 0.0f);
}

void test_vec2_mul() {
  EXPECT_VEC2(kl_vec2_mul(kl_vec2(2.0f, 3.0f), kl_vec2(4.0f, 5.0f)), 8.0f, 15.0f);
  EXPECT_VEC2(kl_vec2_mul(kl_vec2(9.0f, 9.0f), kl_vec2(0.0f, 1.0f)), 0.0f, 9.0f);
}

void test_vec2_div() {
  EXPECT_VEC2(kl_vec2_div(kl_vec2(8.0f, 9.0f), kl_vec2(2.0f, 3.0f)), 4.0f, 3.0f);
  EXPECT_VEC2(kl_vec2_div(kl_vec2(5.0f, 7.0f), kl_vec2(1.0f, 1.0f)), 5.0f, 7.0f);
}

void test_vec2_lerp() {
  KL_Vec2 a = kl_vec2(0.0f, 0.0f);
  KL_Vec2 b = kl_vec2(10.0f, 20.0f);
  EXPECT_VEC2(kl_vec2_lerp(a, b, 0.0f), 0.0f, 0.0f);
  EXPECT_VEC2(kl_vec2_lerp(a, b, 1.0f), 10.0f, 20.0f);
  EXPECT_VEC2(kl_vec2_lerp(a, b, 0.5f), 5.0f, 10.0f);
  EXPECT_VEC2(kl_vec2_lerp(a, b, 0.25f), 2.5f, 5.0f);
}

/* ===================== vec3 tests ===================== */

void test_vec3_constructors() {
  EXPECT_VEC3(kl_vec3(1.0f, 2.0f, 3.0f), 1.0f, 2.0f, 3.0f);
  EXPECT_VEC3(kl_vec31(5.0f), 5.0f, 5.0f, 5.0f);

  KL_Vec3 a = kl_vec3(3.0f, 4.0f, 5.0f);
  KL_TEST_EXPECT_EQ_FLOAT(a.items[0], 3.0f, VEC_EPS);
  KL_TEST_EXPECT_EQ_FLOAT(a.items[1], 4.0f, VEC_EPS);
  KL_TEST_EXPECT_EQ_FLOAT(a.items[2], 5.0f, VEC_EPS);
}

void test_vec3_mag() {
  // 2^2 + 3^2 + 6^2 = 49.
  KL_TEST_EXPECT_EQ_FLOAT(kl_vec3_mag(kl_vec3(2.0f, 3.0f, 6.0f)), 7.0f, VEC_EPS);
  KL_TEST_EXPECT_EQ_FLOAT(kl_vec3_mag(kl_vec31(0.0f)), 0.0f, VEC_EPS);
}

void test_vec3_dot() {
  KL_TEST_EXPECT_EQ_FLOAT(kl_vec3_dot(kl_vec3(1.0f, 2.0f, 3.0f), kl_vec3(4.0f, 5.0f, 6.0f)), 32.0f, VEC_EPS);
  KL_TEST_EXPECT_EQ_FLOAT(kl_vec3_dot(kl_vec3(1.0f, 0.0f, 0.0f), kl_vec3(0.0f, 1.0f, 0.0f)), 0.0f, VEC_EPS);
  KL_Vec3 a = kl_vec3(2.0f, 3.0f, 6.0f);
  KL_TEST_EXPECT_EQ_FLOAT(kl_vec3_dot(a, a), 49.0f, VEC_EPS);
}

void test_vec3_unit() {
  KL_Vec3 u = kl_vec3_unit(kl_vec3(0.0f, 3.0f, 4.0f));
  EXPECT_VEC3(u, 0.0f, 0.6f, 0.8f);
  KL_TEST_EXPECT_EQ_FLOAT(kl_vec3_mag(u), 1.0f, VEC_EPS);

  EXPECT_VEC3(kl_vec3_unit(kl_vec31(0.0f)), 0.0f, 0.0f, 0.0f);
}

void test_vec3_scale() {
  EXPECT_VEC3(kl_vec3_scale(kl_vec3(1.0f, -2.0f, 3.0f), 2.0f), 2.0f, -4.0f, 6.0f);
  EXPECT_VEC3(kl_vec3_scale(kl_vec3(5.0f, 7.0f, 9.0f), 0.0f), 0.0f, 0.0f, 0.0f);
}

void test_vec3_add() {
  EXPECT_VEC3(kl_vec3_add(kl_vec3(1.0f, 2.0f, 3.0f), kl_vec3(4.0f, 5.0f, 6.0f)), 5.0f, 7.0f, 9.0f);
}

void test_vec3_sub() {
  EXPECT_VEC3(kl_vec3_sub(kl_vec3(4.0f, 5.0f, 6.0f), kl_vec3(1.0f, 2.0f, 3.0f)), 3.0f, 3.0f, 3.0f);
}

void test_vec3_mul() {
  EXPECT_VEC3(kl_vec3_mul(kl_vec3(2.0f, 3.0f, 4.0f), kl_vec3(5.0f, 6.0f, 7.0f)), 10.0f, 18.0f, 28.0f);
}

void test_vec3_div() {
  EXPECT_VEC3(kl_vec3_div(kl_vec3(10.0f, 18.0f, 28.0f), kl_vec3(5.0f, 6.0f, 7.0f)), 2.0f, 3.0f, 4.0f);
}

void test_vec3_lerp() {
  KL_Vec3 a = kl_vec3(0.0f, 0.0f, 0.0f);
  KL_Vec3 b = kl_vec3(10.0f, 20.0f, 40.0f);
  EXPECT_VEC3(kl_vec3_lerp(a, b, 0.0f), 0.0f, 0.0f, 0.0f);
  EXPECT_VEC3(kl_vec3_lerp(a, b, 1.0f), 10.0f, 20.0f, 40.0f);
  EXPECT_VEC3(kl_vec3_lerp(a, b, 0.5f), 5.0f, 10.0f, 20.0f);
}

/* ===================== vec4 tests ===================== */

void test_vec4_constructors() {
  EXPECT_VEC4(kl_vec4(1.0f, 2.0f, 3.0f, 4.0f), 1.0f, 2.0f, 3.0f, 4.0f);
  EXPECT_VEC4(kl_vec41(5.0f), 5.0f, 5.0f, 5.0f, 5.0f);

  KL_Vec4 a = kl_vec4(3.0f, 4.0f, 5.0f, 6.0f);
  KL_TEST_EXPECT_EQ_FLOAT(a.items[0], 3.0f, VEC_EPS);
  KL_TEST_EXPECT_EQ_FLOAT(a.items[1], 4.0f, VEC_EPS);
  KL_TEST_EXPECT_EQ_FLOAT(a.items[2], 5.0f, VEC_EPS);
  KL_TEST_EXPECT_EQ_FLOAT(a.items[3], 6.0f, VEC_EPS);
}

void test_vec4_mag() {
  // 1^2 + 2^2 + 2^2 + 4^2 = 25.
  KL_TEST_EXPECT_EQ_FLOAT(kl_vec4_mag(kl_vec4(1.0f, 2.0f, 2.0f, 4.0f)), 5.0f, VEC_EPS);
  KL_TEST_EXPECT_EQ_FLOAT(kl_vec4_mag(kl_vec41(0.0f)), 0.0f, VEC_EPS);
}

void test_vec4_dot() {
  KL_TEST_EXPECT_EQ_FLOAT(kl_vec4_dot(kl_vec4(1.0f, 2.0f, 3.0f, 4.0f), kl_vec4(5.0f, 6.0f, 7.0f, 8.0f)), 70.0f, VEC_EPS);
  KL_TEST_EXPECT_EQ_FLOAT(kl_vec4_dot(kl_vec4(1.0f, 0.0f, 0.0f, 0.0f), kl_vec4(0.0f, 1.0f, 0.0f, 0.0f)), 0.0f, VEC_EPS);
  KL_Vec4 a = kl_vec4(1.0f, 2.0f, 2.0f, 4.0f);
  KL_TEST_EXPECT_EQ_FLOAT(kl_vec4_dot(a, a), 25.0f, VEC_EPS);
}

void test_vec4_unit() {
  KL_Vec4 u = kl_vec4_unit(kl_vec4(0.0f, 0.0f, 3.0f, 4.0f));
  EXPECT_VEC4(u, 0.0f, 0.0f, 0.6f, 0.8f);
  KL_TEST_EXPECT_EQ_FLOAT(kl_vec4_mag(u), 1.0f, VEC_EPS);

  EXPECT_VEC4(kl_vec4_unit(kl_vec41(0.0f)), 0.0f, 0.0f, 0.0f, 0.0f);
}

void test_vec4_scale() {
  EXPECT_VEC4(kl_vec4_scale(kl_vec4(1.0f, -2.0f, 3.0f, -4.0f), 2.0f), 2.0f, -4.0f, 6.0f, -8.0f);
  EXPECT_VEC4(kl_vec4_scale(kl_vec4(5.0f, 7.0f, 9.0f, 11.0f), 0.0f), 0.0f, 0.0f, 0.0f, 0.0f);
}

void test_vec4_add() {
  EXPECT_VEC4(kl_vec4_add(kl_vec4(1.0f, 2.0f, 3.0f, 4.0f), kl_vec4(5.0f, 6.0f, 7.0f, 8.0f)), 6.0f, 8.0f, 10.0f, 12.0f);
}

void test_vec4_sub() {
  EXPECT_VEC4(kl_vec4_sub(kl_vec4(5.0f, 6.0f, 7.0f, 8.0f), kl_vec4(1.0f, 2.0f, 3.0f, 4.0f)), 4.0f, 4.0f, 4.0f, 4.0f);
}

void test_vec4_mul() {
  EXPECT_VEC4(kl_vec4_mul(kl_vec4(2.0f, 3.0f, 4.0f, 5.0f), kl_vec4(6.0f, 7.0f, 8.0f, 9.0f)), 12.0f, 21.0f, 32.0f, 45.0f);
}

void test_vec4_div() {
  EXPECT_VEC4(kl_vec4_div(kl_vec4(12.0f, 21.0f, 32.0f, 45.0f), kl_vec4(6.0f, 7.0f, 8.0f, 9.0f)), 2.0f, 3.0f, 4.0f, 5.0f);
}

void test_vec4_lerp() {
  KL_Vec4 a = kl_vec4(0.0f, 0.0f, 0.0f, 0.0f);
  KL_Vec4 b = kl_vec4(10.0f, 20.0f, 40.0f, 80.0f);
  EXPECT_VEC4(kl_vec4_lerp(a, b, 0.0f), 0.0f, 0.0f, 0.0f, 0.0f);
  EXPECT_VEC4(kl_vec4_lerp(a, b, 1.0f), 10.0f, 20.0f, 40.0f, 80.0f);
  EXPECT_VEC4(kl_vec4_lerp(a, b, 0.5f), 5.0f, 10.0f, 20.0f, 40.0f);
}
