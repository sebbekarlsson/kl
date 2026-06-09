#ifndef KL_MATN_H
#define KL_MATN_H

#include <stdint.h>
#include <stdbool.h>
#include <math.h>

#include <kl/memory/allocator.h>
#include <kl/math/vecn.h>

typedef struct {
  float *items;
  int64_t rows;
  int64_t cols;
  KL_Allocator* allocator;
} KL_MatN_f32;


KL_MatN_f32 kl_matn_f32_create(const float *items, int64_t rows, int64_t cols, KL_Allocator* allocator);
KL_MatN_f32 kl_matn_f32_copy(KL_MatN_f32 src);
KL_MatN_f32 kl_matn_f32_identity(int64_t n, KL_Allocator* allocator);

KL_VecN_f32 kl_matn_f32_get_row(KL_MatN_f32 mat, int64_t row);
KL_VecN_f32 kl_matn_f32_copy_row(KL_MatN_f32 mat, int64_t row);

float kl_matn_f32_at(KL_MatN_f32 mat, int64_t row, int64_t col);
void kl_matn_f32_set(KL_MatN_f32* mat, int64_t row, int64_t col, float value);

bool kl_matn_f32_equals(KL_MatN_f32 a, KL_MatN_f32 b, float epsilon);

// Linear algebra. These allocate a fresh result (shape generally differs from
// the inputs) and so have no in-place variant.
KL_MatN_f32 kl_matn_f32_transpose(KL_MatN_f32 mat);
KL_MatN_f32 kl_matn_f32_mul(KL_MatN_f32 a, KL_MatN_f32 b);
// Non-allocating matrix multiply: writes a*b into the caller-owned `out`, whose
// shape must already be a.rows x b.cols. `out` must not alias `a` or `b`.
void kl_matn_f32_mul_into(KL_MatN_f32 a, KL_MatN_f32 b, KL_MatN_f32* out);
KL_VecN_f32 kl_matn_f32_mul_vec(KL_MatN_f32 mat, KL_VecN_f32 v);


// *_ip = in-place (No allocations)

// scale/negate/abs/clamp are unary: the _into form writes the transform of `a`
// into the caller-owned `out` (same shape; out may alias a). lerp is binary, so
// like the matrix-matrix ops its `out` must not alias `b`.
void kl_matn_f32_scale_ip(KL_MatN_f32* a, float s);
void kl_matn_f32_scale_into(KL_MatN_f32 a, float s, KL_MatN_f32* out);
KL_MatN_f32 kl_matn_f32_scale(KL_MatN_f32 a, float s);
void kl_matn_f32_negate_ip(KL_MatN_f32* a);
void kl_matn_f32_negate_into(KL_MatN_f32 a, KL_MatN_f32* out);
KL_MatN_f32 kl_matn_f32_negate(KL_MatN_f32 a);
void kl_matn_f32_abs_ip(KL_MatN_f32* a);
void kl_matn_f32_abs_into(KL_MatN_f32 a, KL_MatN_f32* out);
KL_MatN_f32 kl_matn_f32_abs(KL_MatN_f32 a);
void kl_matn_f32_clamp_ip(KL_MatN_f32* a, float lo, float hi);
void kl_matn_f32_clamp_into(KL_MatN_f32 a, float lo, float hi, KL_MatN_f32* out);
KL_MatN_f32 kl_matn_f32_clamp(KL_MatN_f32 a, float lo, float hi);
void kl_matn_f32_lerp_ip(KL_MatN_f32* a, KL_MatN_f32 b, float t);
void kl_matn_f32_lerp_into(KL_MatN_f32 a, KL_MatN_f32 b, float t, KL_MatN_f32* out);
KL_MatN_f32 kl_matn_f32_lerp(KL_MatN_f32 a, KL_MatN_f32 b, float t);

// Elementwise matrix-matrix ops. Each comes in three forms: _ip writes the
// result into `a`; _into writes `a OP b` into the caller-owned `out` (whose
// shape must match and which must not alias `b` -- aliasing `a` is fine); and
// the allocating form returns a fresh matrix carrying `a`'s allocator.
void kl_matn_f32_add_ip(KL_MatN_f32* a, KL_MatN_f32 b);
void kl_matn_f32_add_into(KL_MatN_f32 a, KL_MatN_f32 b, KL_MatN_f32* out);
KL_MatN_f32 kl_matn_f32_add(KL_MatN_f32 a, KL_MatN_f32 b);
void kl_matn_f32_sub_ip(KL_MatN_f32* a, KL_MatN_f32 b);
void kl_matn_f32_sub_into(KL_MatN_f32 a, KL_MatN_f32 b, KL_MatN_f32* out);
KL_MatN_f32 kl_matn_f32_sub(KL_MatN_f32 a, KL_MatN_f32 b);
void kl_matn_f32_div_ip(KL_MatN_f32* a, KL_MatN_f32 b);
void kl_matn_f32_div_into(KL_MatN_f32 a, KL_MatN_f32 b, KL_MatN_f32* out);
KL_MatN_f32 kl_matn_f32_div(KL_MatN_f32 a, KL_MatN_f32 b);
// Elementwise (Hadamard) product. Real matrix multiplication is kl_matn_f32_mul.
void kl_matn_f32_mul_elementwise_ip(KL_MatN_f32* a, KL_MatN_f32 b);
void kl_matn_f32_mul_elementwise_into(KL_MatN_f32 a, KL_MatN_f32 b, KL_MatN_f32* out);
KL_MatN_f32 kl_matn_f32_mul_elementwise(KL_MatN_f32 a, KL_MatN_f32 b);

#endif
