#include <kl/math/matn.h>
#include <kl/macros.h>
#include <kl/common/number.h>
#include <assert.h>
#include <string.h>

#if defined(__AVX2__) && defined(__FMA__)
#define KL_MATN_SIMD 1
#include <immintrin.h>
#else
#define KL_MATN_SIMD 0
#endif

// Matches vecn: 8 floats per AVX2 register, unaligned load/store so we can wrap
// memory we did not allocate. The SIMD loop runs while a full 8-lane block
// remains; the scalar tail handles the remainder and is the full fallback when
// AVX2/FMA are unavailable.
#define KL_MATN_SIMD_LANES 8


#define ASSERT_VALID_MATN(V) do { assert((V).items != 0); assert((V).rows > 0); assert((V).cols > 0); } while (0)

static inline int64_t index_at(KL_MatN_f32 mat, int64_t row, int64_t col) {
  ASSERT_VALID_MATN(mat);
  assert(row >= 0);
  assert(row < mat.rows);
  assert(col >= 0);
  assert(col < mat.cols);
  return row * mat.cols + col;
}

// A row-major matrix is a contiguous rows*cols float buffer, so the elementwise
// ops are just the vecn ops over that buffer. This non-owning view lets us
// delegate to vecn (and its SIMD paths) without copying.
static inline KL_VecN_f32 matn_as_vecn(KL_MatN_f32 mat) {
  ASSERT_VALID_MATN(mat);
  return (KL_VecN_f32){
    .allocator = mat.allocator,
    .items = mat.items,
    .length = mat.rows * mat.cols
  };
}


KL_MatN_f32 kl_matn_f32_create(const float *items, int64_t rows, int64_t cols, KL_Allocator* allocator) {
  assert(allocator != 0);
  assert(rows > 0);
  assert(cols > 0);
  int64_t length = rows * cols;
  float* copy = (float*)kl_allocator_malloc(allocator, length * sizeof(float));
  assert(copy != 0);

  if (items != 0) {
    memcpy(copy, items, length * sizeof(float));
  }

  return (KL_MatN_f32) {
    .allocator = allocator,
    .items = copy,
    .rows = rows,
    .cols = cols
  };
}

KL_MatN_f32 kl_matn_f32_copy(KL_MatN_f32 src) {
  assert(src.allocator != 0);
  ASSERT_VALID_MATN(src);
  return kl_matn_f32_create(src.items, src.rows, src.cols, src.allocator);
}


KL_VecN_f32 kl_matn_f32_get_row(KL_MatN_f32 mat, int64_t row) {
  ASSERT_VALID_MATN(mat);
  int64_t idx = row * mat.cols + 0;
  assert(idx >= 0);
  assert(idx < (mat.rows * mat.cols));

  return (KL_VecN_f32){
    .allocator = mat.allocator,
    .items = &mat.items[idx],
    .length = mat.cols
  };
}

KL_VecN_f32 kl_matn_f32_copy_row(KL_MatN_f32 mat, int64_t row) {
  ASSERT_VALID_MATN(mat);
  return kl_vecn_f32_copy(kl_matn_f32_get_row(mat, row));
}

float kl_matn_f32_at(KL_MatN_f32 mat, int64_t row, int64_t col) {
  ASSERT_VALID_MATN(mat);
  return mat.items[index_at(mat, row, col)];
}

void kl_matn_f32_set(KL_MatN_f32 mat, int64_t row, int64_t col, float value) {
  ASSERT_VALID_MATN(mat);
  // mat is passed by value, but .items aliases the caller's backing store, so
  // the write is visible through any handle that shares it.
  mat.items[index_at(mat, row, col)] = value;
}

KL_MatN_f32 kl_matn_f32_identity(int64_t n, KL_Allocator* allocator) {
  assert(allocator != 0);
  assert(n > 0);
  // create() with null items leaves the buffer uninitialized; zero it first,
  // then drop 1s on the diagonal.
  KL_MatN_f32 m = kl_matn_f32_create(0, n, n, allocator);
  memset(m.items, 0, (size_t)(n * n) * sizeof(float));
  for (int64_t i = 0; i < n; i++) m.items[i * n + i] = 1.0f;
  return m;
}

bool kl_matn_f32_equals(KL_MatN_f32 a, KL_MatN_f32 b, float epsilon) {
  ASSERT_VALID_MATN(a);
  ASSERT_VALID_MATN(b);
  // Same element count but a different shape is not equal.
  if (a.rows != b.rows || a.cols != b.cols) return false;
  return kl_vecn_f32_equals(matn_as_vecn(a), matn_as_vecn(b), epsilon);
}

KL_MatN_f32 kl_matn_f32_transpose(KL_MatN_f32 mat) {
  ASSERT_VALID_MATN(mat);
  KL_MatN_f32 out = kl_matn_f32_create(0, mat.cols, mat.rows, mat.allocator);
  for (int64_t r = 0; r < mat.rows; r++) {
    for (int64_t c = 0; c < mat.cols; c++) {
      out.items[c * out.cols + r] = mat.items[r * mat.cols + c];
    }
  }
  return out;
}

KL_MatN_f32 kl_matn_f32_mul(KL_MatN_f32 a, KL_MatN_f32 b) {
  ASSERT_VALID_MATN(a);
  ASSERT_VALID_MATN(b);
  assert(a.cols == b.rows);
  KL_MatN_f32 out = kl_matn_f32_create(0, a.rows, b.cols, a.allocator);

  // ikj ordering: for each output row i we sweep k and add a[i][k] * (row k of
  // b) into (row i of out). Both b's row k and out's row i are contiguous, so
  // the inner j loop is a broadcast-multiply-accumulate that vectorizes the way
  // the vecn ops do -- unlike the textbook ijk loop, whose inner stride walks a
  // column of b. out is accumulated into, so it must start zeroed.
  const int64_t n = b.cols;  // == out.cols
  memset(out.items, 0, (size_t)(out.rows * out.cols) * sizeof(float));

  for (int64_t i = 0; i < a.rows; i++) {
    float* orow = &out.items[i * out.cols];
    for (int64_t k = 0; k < a.cols; k++) {
      const float aik = a.items[i * a.cols + k];
      const float* brow = &b.items[k * b.cols];
      int64_t j = 0;
#if KL_MATN_SIMD
      __m256 vaik = _mm256_set1_ps(aik);
      for (; j + KL_MATN_SIMD_LANES <= n; j += KL_MATN_SIMD_LANES) {
        __m256 vo = _mm256_loadu_ps(&orow[j]);
        __m256 vb = _mm256_loadu_ps(&brow[j]);
        _mm256_storeu_ps(&orow[j], _mm256_fmadd_ps(vaik, vb, vo));
      }
#endif
      for (; j < n; j++) orow[j] += aik * brow[j];
    }
  }
  return out;
}

KL_VecN_f32 kl_matn_f32_mul_vec(KL_MatN_f32 mat, KL_VecN_f32 v) {
  ASSERT_VALID_MATN(mat);
  assert(v.items != 0);
  assert(v.length > 0);
  assert(mat.cols == v.length);
  KL_VecN_f32 out = kl_vecn_f32_create(0, mat.rows, mat.allocator);
  // Each output element is the dot of a (contiguous) matrix row with v, so we
  // reuse vecn's SIMD dot product over a non-owning row view.
  for (int64_t i = 0; i < mat.rows; i++) {
    out.items[i] = kl_vecn_f32_dot(kl_matn_f32_get_row(mat, i), v);
  }
  return out;
}

// --- Elementwise ops -------------------------------------------------------
// A matrix is a flat float buffer, so every elementwise op is exactly the vecn
// op over rows*cols elements. We wrap items in a non-owning vecn view and
// delegate, inheriting vecn's SIMD paths instead of duplicating them.

void kl_matn_f32_scale_ip(KL_MatN_f32* a, float s) {
  ASSERT_VALID_MATN(*a);
  KL_VecN_f32 v = matn_as_vecn(*a);
  kl_vecn_f32_scale_ip(&v, s);
}

KL_MatN_f32 kl_matn_f32_scale(KL_MatN_f32 a, float s) {
  KL_MatN_f32 m = kl_matn_f32_copy(a);
  kl_matn_f32_scale_ip(&m, s);
  return m;
}

void kl_matn_f32_negate_ip(KL_MatN_f32* a) {
  ASSERT_VALID_MATN(*a);
  KL_VecN_f32 v = matn_as_vecn(*a);
  kl_vecn_f32_negate_ip(&v);
}

KL_MatN_f32 kl_matn_f32_negate(KL_MatN_f32 a) {
  KL_MatN_f32 m = kl_matn_f32_copy(a);
  kl_matn_f32_negate_ip(&m);
  return m;
}

void kl_matn_f32_abs_ip(KL_MatN_f32* a) {
  ASSERT_VALID_MATN(*a);
  KL_VecN_f32 v = matn_as_vecn(*a);
  kl_vecn_f32_abs_ip(&v);
}

KL_MatN_f32 kl_matn_f32_abs(KL_MatN_f32 a) {
  KL_MatN_f32 m = kl_matn_f32_copy(a);
  kl_matn_f32_abs_ip(&m);
  return m;
}

void kl_matn_f32_clamp_ip(KL_MatN_f32* a, float lo, float hi) {
  ASSERT_VALID_MATN(*a);
  KL_VecN_f32 v = matn_as_vecn(*a);
  kl_vecn_f32_clamp_ip(&v, lo, hi);
}

KL_MatN_f32 kl_matn_f32_clamp(KL_MatN_f32 a, float lo, float hi) {
  KL_MatN_f32 m = kl_matn_f32_copy(a);
  kl_matn_f32_clamp_ip(&m, lo, hi);
  return m;
}

void kl_matn_f32_lerp_ip(KL_MatN_f32* a, KL_MatN_f32 b, float t) {
  ASSERT_VALID_MATN(*a);
  ASSERT_VALID_MATN(b);
  assert(a->rows == b.rows);
  assert(a->cols == b.cols);
  KL_VecN_f32 va = matn_as_vecn(*a);
  kl_vecn_f32_lerp_ip(&va, matn_as_vecn(b), t);
}

KL_MatN_f32 kl_matn_f32_lerp(KL_MatN_f32 a, KL_MatN_f32 b, float t) {
  KL_MatN_f32 m = kl_matn_f32_copy(a);
  kl_matn_f32_lerp_ip(&m, b, t);
  return m;
}

// The four elementwise matrix-matrix ops share one shape: validate matching
// dimensions, then delegate to the matching vecn binop over flat views.
#define KL_MATN__DEFINE_BINOP_IP(NAME)                          \
  void kl_matn_f32_##NAME##_ip(KL_MatN_f32* a, KL_MatN_f32 b) { \
    ASSERT_VALID_MATN(*a);                                      \
    ASSERT_VALID_MATN(b);                                       \
    assert(a->rows == b.rows);                                  \
    assert(a->cols == b.cols);                                  \
    KL_VecN_f32 va = matn_as_vecn(*a);                          \
    kl_vecn_f32_##NAME##_ip(&va, matn_as_vecn(b));              \
  }                                                             \
  KL_MatN_f32 kl_matn_f32_##NAME(KL_MatN_f32 a, KL_MatN_f32 b) { \
    KL_MatN_f32 m = kl_matn_f32_copy(a);                        \
    kl_matn_f32_##NAME##_ip(&m, b);                             \
    return m;                                                   \
  }

KL_MATN__DEFINE_BINOP_IP(add)
KL_MATN__DEFINE_BINOP_IP(sub)
KL_MATN__DEFINE_BINOP_IP(div)

#undef KL_MATN__DEFINE_BINOP_IP

// Hadamard product. Named *_elementwise to leave kl_matn_f32_mul for real
// matrix multiplication; it maps to vecn's plain elementwise mul, so it can't
// ride the macro above (which pastes NAME into the vecn op name).
void kl_matn_f32_mul_elementwise_ip(KL_MatN_f32* a, KL_MatN_f32 b) {
  ASSERT_VALID_MATN(*a);
  ASSERT_VALID_MATN(b);
  assert(a->rows == b.rows);
  assert(a->cols == b.cols);
  KL_VecN_f32 va = matn_as_vecn(*a);
  kl_vecn_f32_mul_ip(&va, matn_as_vecn(b));
}

KL_MatN_f32 kl_matn_f32_mul_elementwise(KL_MatN_f32 a, KL_MatN_f32 b) {
  KL_MatN_f32 m = kl_matn_f32_copy(a);
  kl_matn_f32_mul_elementwise_ip(&m, b);
  return m;
}
