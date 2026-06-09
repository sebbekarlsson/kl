#include <kl/math/vecn.h>
#include <kl/common/number.h>
#include <assert.h>
#include <string.h>
#include <math.h>

#if defined(__AVX2__) && defined(__FMA__)
#define KL_VECN_SIMD 1
#include <immintrin.h>
#else
#define KL_VECN_SIMD 0
#endif

// All SIMD memory access uses the unaligned load/store intrinsics
// (_mm256_loadu_ps / _mm256_storeu_ps), which never fault on unaligned data and
// cost the same as aligned access on AVX2 hardware. So nothing here requires an
// aligned allocation -- which also lets a KL_VecN_f32 freely wrap memory we did
// not allocate (the vec2/vec3/vec4 helpers wrap stack arrays; matn hands out row
// slices). The SIMD loops only run while a full 8-lane block remains; the
// trailing scalar loop handles the remainder and also serves as the complete
// fallback when AVX2/FMA are unavailable.
#define KL_VECN_SIMD_LANES 8


#define ASSERT_VALID_VECN(V) do { assert((V).items != 0); assert((V).length > 0); } while (0)


#if KL_VECN_SIMD
// Horizontal sum of the 8 lanes of a 256-bit float register.
static inline float kl_vecn__hsum256(__m256 v) {
  __m128 vlow  = _mm256_castps256_ps128(v);
  __m128 vhigh = _mm256_extractf128_ps(v, 1);
  vlow = _mm_add_ps(vlow, vhigh);              // 4 partial sums
  __m128 shuf = _mm_movehdup_ps(vlow);
  __m128 sums = _mm_add_ps(vlow, shuf);
  shuf = _mm_movehl_ps(shuf, sums);
  sums = _mm_add_ss(sums, shuf);
  return _mm_cvtss_f32(sums);
}
#endif


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

float kl_vecn_f32_mag_sq(KL_VecN_f32 a) {
  ASSERT_VALID_VECN(a);
  float s = 0.0f;
  int64_t i = 0;
#if KL_VECN_SIMD
  __m256 acc = _mm256_setzero_ps();
  for (; i + KL_VECN_SIMD_LANES <= a.length; i += KL_VECN_SIMD_LANES) {
    __m256 va = _mm256_loadu_ps(&a.items[i]);
    acc = _mm256_fmadd_ps(va, va, acc);
  }
  s += kl_vecn__hsum256(acc);
#endif
  for (; i < a.length; i++) s += a.items[i] * a.items[i];
  return s;
}

float kl_vecn_f32_mag(KL_VecN_f32 a) {
  return sqrtf(kl_vecn_f32_mag_sq(a));
}

float kl_vecn_f32_sum(KL_VecN_f32 a) {
  ASSERT_VALID_VECN(a);
  float s = 0.0f;
  int64_t i = 0;
#if KL_VECN_SIMD
  __m256 acc = _mm256_setzero_ps();
  for (; i + KL_VECN_SIMD_LANES <= a.length; i += KL_VECN_SIMD_LANES) {
    acc = _mm256_add_ps(acc, _mm256_loadu_ps(&a.items[i]));
  }
  s += kl_vecn__hsum256(acc);
#endif
  for (; i < a.length; i++) s += a.items[i];
  return s;
}

float kl_vecn_f32_dot(KL_VecN_f32 a, KL_VecN_f32 b) {
  ASSERT_VALID_VECN(a);
  ASSERT_VALID_VECN(b);
  assert(a.length == b.length);
  float s = 0.0f;
  int64_t i = 0;
#if KL_VECN_SIMD
  __m256 acc = _mm256_setzero_ps();
  for (; i + KL_VECN_SIMD_LANES <= a.length; i += KL_VECN_SIMD_LANES) {
    __m256 va = _mm256_loadu_ps(&a.items[i]);
    __m256 vb = _mm256_loadu_ps(&b.items[i]);
    acc = _mm256_fmadd_ps(va, vb, acc);
  }
  s += kl_vecn__hsum256(acc);
#endif
  for (; i < a.length; i++) s += a.items[i] * b.items[i];
  return s;
}

float kl_vecn_f32_distance_sq(KL_VecN_f32 a, KL_VecN_f32 b) {
  ASSERT_VALID_VECN(a);
  ASSERT_VALID_VECN(b);
  assert(a.length == b.length);

  float s = 0.0f;
  int64_t i = 0;
#if KL_VECN_SIMD
  __m256 acc = _mm256_setzero_ps();
  for (; i + KL_VECN_SIMD_LANES <= a.length; i += KL_VECN_SIMD_LANES) {
    __m256 d = _mm256_sub_ps(_mm256_loadu_ps(&b.items[i]), _mm256_loadu_ps(&a.items[i]));
    acc = _mm256_fmadd_ps(d, d, acc);
  }
  s += kl_vecn__hsum256(acc);
#endif
  for (; i < a.length; i++) {
    float k = b.items[i] - a.items[i];
    s += k * k;
  }
  return s;
}

float kl_vecn_f32_distance(KL_VecN_f32 a, KL_VecN_f32 b) {
  return sqrtf(kl_vecn_f32_distance_sq(a, b));
}

float kl_vecn_f32_manhattan_distance(KL_VecN_f32 a, KL_VecN_f32 b) {
  ASSERT_VALID_VECN(a);
  ASSERT_VALID_VECN(b);
  assert(a.length == b.length);

  float s = 0.0f;
  int64_t i = 0;
#if KL_VECN_SIMD
  const __m256 absmask = _mm256_set1_ps(-0.0f);  // clearing this bit yields |x|
  __m256 acc = _mm256_setzero_ps();
  for (; i + KL_VECN_SIMD_LANES <= a.length; i += KL_VECN_SIMD_LANES) {
    __m256 d = _mm256_sub_ps(_mm256_loadu_ps(&b.items[i]), _mm256_loadu_ps(&a.items[i]));
    acc = _mm256_add_ps(acc, _mm256_andnot_ps(absmask, d));
  }
  s += kl_vecn__hsum256(acc);
#endif
  for (; i < a.length; i++) s += fabsf(b.items[i] - a.items[i]);
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

bool kl_vecn_f32_equals(KL_VecN_f32 a, KL_VecN_f32 b, float epsilon) {
  ASSERT_VALID_VECN(a);
  ASSERT_VALID_VECN(b);

  // Vectors of differing length are simply not equal.
  if (a.length != b.length) return false;

  int64_t i = 0;
#if KL_VECN_SIMD
  const __m256 absmask = _mm256_set1_ps(-0.0f);
  __m256 veps = _mm256_set1_ps(epsilon);
  for (; i + KL_VECN_SIMD_LANES <= a.length; i += KL_VECN_SIMD_LANES) {
    __m256 d = _mm256_andnot_ps(absmask, _mm256_sub_ps(_mm256_loadu_ps(&a.items[i]),
                                                       _mm256_loadu_ps(&b.items[i])));
    __m256 gt = _mm256_cmp_ps(d, veps, _CMP_GT_OQ);
    if (_mm256_movemask_ps(gt) != 0) return false;
  }
#endif
  for (; i < a.length; i++) {
    if (fabsf(a.items[i] - b.items[i]) > epsilon) return false;
  }
  return true;
}

void kl_vecn_f32_unit_ip(KL_VecN_f32* a) {
  ASSERT_VALID_VECN(*a);
  float mag = kl_vecn_f32_mag(*a);
  if (mag <= 0.0f) return;
  kl_vecn_f32_scale_ip(a, 1.0f / mag);
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
  int64_t i = 0;
#if KL_VECN_SIMD
  __m256 vs = _mm256_set1_ps(s);
  for (; i + KL_VECN_SIMD_LANES <= a->length; i += KL_VECN_SIMD_LANES) {
    _mm256_storeu_ps(&a->items[i], _mm256_mul_ps(_mm256_loadu_ps(&a->items[i]), vs));
  }
#endif
  for (; i < a->length; i++) a->items[i] = a->items[i] * s;
}

KL_VecN_f32 kl_vecn_f32_scale(KL_VecN_f32 a, float s) {
  ASSERT_VALID_VECN(a);
  KL_VecN_f32 v = kl_vecn_f32_copy(a);
  kl_vecn_f32_scale_ip(&v, s);
  return v;
}

void kl_vecn_f32_negate_ip(KL_VecN_f32* a) {
  ASSERT_VALID_VECN(*a);
  int64_t i = 0;
#if KL_VECN_SIMD
  const __m256 signmask = _mm256_set1_ps(-0.0f);  // flipping this bit negates
  for (; i + KL_VECN_SIMD_LANES <= a->length; i += KL_VECN_SIMD_LANES) {
    _mm256_storeu_ps(&a->items[i], _mm256_xor_ps(_mm256_loadu_ps(&a->items[i]), signmask));
  }
#endif
  for (; i < a->length; i++) a->items[i] = -a->items[i];
}

KL_VecN_f32 kl_vecn_f32_negate(KL_VecN_f32 a) {
  ASSERT_VALID_VECN(a);
  KL_VecN_f32 v = kl_vecn_f32_copy(a);
  kl_vecn_f32_negate_ip(&v);
  return v;
}

void kl_vecn_f32_abs_ip(KL_VecN_f32* a) {
  ASSERT_VALID_VECN(*a);
  int64_t i = 0;
#if KL_VECN_SIMD
  const __m256 absmask = _mm256_set1_ps(-0.0f);  // clearing this bit yields |x|
  for (; i + KL_VECN_SIMD_LANES <= a->length; i += KL_VECN_SIMD_LANES) {
    _mm256_storeu_ps(&a->items[i], _mm256_andnot_ps(absmask, _mm256_loadu_ps(&a->items[i])));
  }
#endif
  for (; i < a->length; i++) a->items[i] = fabsf(a->items[i]);
}

KL_VecN_f32 kl_vecn_f32_abs(KL_VecN_f32 a) {
  ASSERT_VALID_VECN(a);
  KL_VecN_f32 v = kl_vecn_f32_copy(a);
  kl_vecn_f32_abs_ip(&v);
  return v;
}

void kl_vecn_f32_clamp_ip(KL_VecN_f32* a, float lo, float hi) {
  ASSERT_VALID_VECN(*a);
  int64_t i = 0;
#if KL_VECN_SIMD
  __m256 vlo = _mm256_set1_ps(lo);
  __m256 vhi = _mm256_set1_ps(hi);
  // Mirrors kl_clamp_f32: fminf(fmaxf(v, lo), hi).
  for (; i + KL_VECN_SIMD_LANES <= a->length; i += KL_VECN_SIMD_LANES) {
    __m256 v = _mm256_loadu_ps(&a->items[i]);
    _mm256_storeu_ps(&a->items[i], _mm256_min_ps(_mm256_max_ps(v, vlo), vhi));
  }
#endif
  for (; i < a->length; i++) a->items[i] = kl_clamp_f32(a->items[i], lo, hi);
}

KL_VecN_f32 kl_vecn_f32_clamp(KL_VecN_f32 a, float lo, float hi) {
  ASSERT_VALID_VECN(a);
  KL_VecN_f32 v = kl_vecn_f32_copy(a);
  kl_vecn_f32_clamp_ip(&v, lo, hi);
  return v;
}

void kl_vecn_f32_lerp_ip(KL_VecN_f32* a, KL_VecN_f32 b, float t) {
  ASSERT_VALID_VECN(*a);
  ASSERT_VALID_VECN(b);
  assert(a->length == b.length);
  int64_t i = 0;
#if KL_VECN_SIMD
  __m256 vt = _mm256_set1_ps(t);
  __m256 vomt = _mm256_set1_ps(1.0f - t);
  // Mirrors kl_lerp_f32: (1 - t)*a + t*b.
  for (; i + KL_VECN_SIMD_LANES <= a->length; i += KL_VECN_SIMD_LANES) {
    __m256 va = _mm256_loadu_ps(&a->items[i]);
    __m256 vb = _mm256_loadu_ps(&b.items[i]);
    _mm256_storeu_ps(&a->items[i], _mm256_fmadd_ps(vt, vb, _mm256_mul_ps(vomt, va)));
  }
#endif
  for (; i < a->length; i++) a->items[i] = kl_lerp_f32(a->items[i], b.items[i], t);
}

KL_VecN_f32 kl_vecn_f32_lerp(KL_VecN_f32 a, KL_VecN_f32 b, float t) {
  KL_VecN_f32 v = kl_vecn_f32_copy(a);
  kl_vecn_f32_lerp_ip(&v, b, t);
  return v;
}

// The elementwise binary in-place ops share one shape: load a, load b, store the
// SIMD op, then a scalar tail. Generated from a macro so the four stay in sync.
#if KL_VECN_SIMD
#define KL_VECN__DEFINE_BINOP_IP(NAME, SCALAR_OP, SIMD_FN)                              \
  void kl_vecn_f32_##NAME##_ip(KL_VecN_f32* a, KL_VecN_f32 b) {                         \
    ASSERT_VALID_VECN(*a);                                                             \
    ASSERT_VALID_VECN(b);                                                              \
    assert(a->length == b.length);                                                    \
    int64_t i = 0;                                                                     \
    for (; i + KL_VECN_SIMD_LANES <= a->length; i += KL_VECN_SIMD_LANES) {             \
      __m256 va = _mm256_loadu_ps(&a->items[i]);                                       \
      __m256 vb = _mm256_loadu_ps(&b.items[i]);                                        \
      _mm256_storeu_ps(&a->items[i], SIMD_FN(va, vb));                                 \
    }                                                                                  \
    for (; i < a->length; i++) a->items[i] = a->items[i] SCALAR_OP b.items[i];         \
  }
#else
#define KL_VECN__DEFINE_BINOP_IP(NAME, SCALAR_OP, SIMD_FN)                              \
  void kl_vecn_f32_##NAME##_ip(KL_VecN_f32* a, KL_VecN_f32 b) {                         \
    ASSERT_VALID_VECN(*a);                                                             \
    ASSERT_VALID_VECN(b);                                                              \
    assert(a->length == b.length);                                                    \
    for (int64_t i = 0; i < a->length; i++) a->items[i] = a->items[i] SCALAR_OP b.items[i]; \
  }
#endif

KL_VECN__DEFINE_BINOP_IP(add, +, _mm256_add_ps)
KL_VECN__DEFINE_BINOP_IP(sub, -, _mm256_sub_ps)
KL_VECN__DEFINE_BINOP_IP(mul, *, _mm256_mul_ps)
KL_VECN__DEFINE_BINOP_IP(div, /, _mm256_div_ps)

#undef KL_VECN__DEFINE_BINOP_IP

KL_VecN_f32 kl_vecn_f32_add(KL_VecN_f32 a, KL_VecN_f32 b) {
  KL_VecN_f32 v = kl_vecn_f32_copy(a);
  kl_vecn_f32_add_ip(&v, b);
  return v;
}

KL_VecN_f32 kl_vecn_f32_sub(KL_VecN_f32 a, KL_VecN_f32 b) {
  KL_VecN_f32 v = kl_vecn_f32_copy(a);
  kl_vecn_f32_sub_ip(&v, b);
  return v;
}

KL_VecN_f32 kl_vecn_f32_div(KL_VecN_f32 a, KL_VecN_f32 b) {
  KL_VecN_f32 v = kl_vecn_f32_copy(a);
  kl_vecn_f32_div_ip(&v, b);
  return v;
}

KL_VecN_f32 kl_vecn_f32_mul(KL_VecN_f32 a, KL_VecN_f32 b) {
  KL_VecN_f32 v = kl_vecn_f32_copy(a);
  kl_vecn_f32_mul_ip(&v, b);
  return v;
}
