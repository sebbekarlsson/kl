#include <kl/math/vecn.h>
#include <assert.h>
#include <string.h>
#include <math.h>


#define ASSERT_VALID_VECN(V) do { assert((V).items != 0); assert((V).length > 0); } while (0)


KL_VecN_f32 kl_vecn_f32_create(const float *items, int64_t length, KL_Allocator* allocator) {
  assert(allocator != 0);

  float* copy = (float*)kl_allocator_malloc(allocator, length * sizeof(float));
  assert(copy != 0);

  if (items != 0) {
    memcpy(copy, items, length * sizeof(float));
  }

  return (KL_VecN_f32){
    .allocator = allocator,
    .items = copy,
    .length = length
  };
}

KL_VecN_f32 kl_vecn_f32_copy(KL_VecN_f32 src) {
  ASSERT_VALID_VECN(src);
  assert(src.allocator != 0);
  return kl_vecn_f32_create(src.items, src.length, src.allocator);
}

float kl_vecn_f32_mag(KL_VecN_f32 a) {
  ASSERT_VALID_VECN(a);
  float s = 0.0f;
  for (int64_t i = 0; i < a.length; i++) s += a.items[i] * a.items[i];
  return sqrtf(s);
}

float kl_vecn_f32_dot(KL_VecN_f32 a, KL_VecN_f32 b) {
  ASSERT_VALID_VECN(a);
  ASSERT_VALID_VECN(b);
  assert(a.length == b.length);
  float s = 0.0f;
  for (int64_t i = 0; i < a.length; i++) s += a.items[i] * b.items[i];
  return s;
}

float kl_vecn_f32_cosine_distance(KL_VecN_f32 a, KL_VecN_f32 b) {
  ASSERT_VALID_VECN(a);
  ASSERT_VALID_VECN(b);
  assert(a.length == b.length);

  // cosine distance = 1 - (a . b) / (|a| * |b|).
  float denom = kl_vecn_f32_mag(a) * kl_vecn_f32_mag(b);
  // A zero-length vector has no direction; treat similarity as 0 (max distance)
  // rather than dividing by zero.
  if (denom <= 0.0f) return 1.0f;

  return 1.0f - (kl_vecn_f32_dot(a, b) / denom);
}

void kl_vecn_f32_unit_ip(KL_VecN_f32* a) {
  ASSERT_VALID_VECN(*a);
  float mag = kl_vecn_f32_mag(*a);
  if (mag <= 0.0f) return;
  for (int64_t i = 0; i < a->length; i++) a->items[i] = a->items[i] / mag;
}

KL_VecN_f32 kl_vecn_f32_unit(KL_VecN_f32 a) {
  ASSERT_VALID_VECN(a);
  assert(a.allocator != 0);

  KL_VecN_f32 v = kl_vecn_f32_copy(a);
  kl_vecn_f32_unit_ip(&v);
  return v;
}
void kl_vecn_f32_scale_ip(KL_VecN_f32* a, float s) {
  ASSERT_VALID_VECN(*a);
  for (int64_t i = 0; i < a->length; i++) a->items[i] = a->items[i] * s;
}

KL_VecN_f32 kl_vecn_f32_scale(KL_VecN_f32 a, float s) {
  ASSERT_VALID_VECN(a);
  KL_VecN_f32 v = kl_vecn_f32_copy(a);
  kl_vecn_f32_scale_ip(&v, s);
  return v;
}

void kl_vecn_f32_add_ip(KL_VecN_f32* a, KL_VecN_f32 b) {
  ASSERT_VALID_VECN(*a);
  ASSERT_VALID_VECN(b);
  assert(a->length == b.length);
  for (int64_t i = 0; i < a->length; i++) a->items[i] = a->items[i] +  b.items[i];
}

KL_VecN_f32 kl_vecn_f32_add(KL_VecN_f32 a, KL_VecN_f32 b) {
  KL_VecN_f32 v = kl_vecn_f32_copy(a);
  kl_vecn_f32_add_ip(&v, b);
  return v;
}

void kl_vecn_f32_sub_ip(KL_VecN_f32* a, KL_VecN_f32 b) {
  ASSERT_VALID_VECN(*a);
  ASSERT_VALID_VECN(b);
  assert(a->length == b.length);
  for (int64_t i = 0; i < a->length; i++) a->items[i] = a->items[i] - b.items[i];
}

KL_VecN_f32 kl_vecn_f32_sub(KL_VecN_f32 a, KL_VecN_f32 b) {
  KL_VecN_f32 v = kl_vecn_f32_copy(a);
  kl_vecn_f32_sub_ip(&v, b);
  return v;
}

void kl_vecn_f32_div_ip(KL_VecN_f32* a, KL_VecN_f32 b) {
  ASSERT_VALID_VECN(*a);
  ASSERT_VALID_VECN(b);
  assert(a->length == b.length);
  for (int64_t i = 0; i < a->length; i++) a->items[i] = a->items[i] / b.items[i];
}

KL_VecN_f32 kl_vecn_f32_div(KL_VecN_f32 a, KL_VecN_f32 b) {
  KL_VecN_f32 v = kl_vecn_f32_copy(a);
  kl_vecn_f32_div_ip(&v, b);
  return v;
}

void kl_vecn_f32_mul_ip(KL_VecN_f32* a, KL_VecN_f32 b) {
  ASSERT_VALID_VECN(*a);
  ASSERT_VALID_VECN(b);
  assert(a->length == b.length);
  for (int64_t i = 0; i < a->length; i++) a->items[i] = a->items[i] * b.items[i];
}

KL_VecN_f32 kl_vecn_f32_mul(KL_VecN_f32 a, KL_VecN_f32 b) {
  KL_VecN_f32 v = kl_vecn_f32_copy(a);
  kl_vecn_f32_mul_ip(&v, b);
  return v;
}
