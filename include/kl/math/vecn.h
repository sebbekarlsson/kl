#ifndef KL_VECN_H
#define KL_VECN_H

#include <stdint.h>
#include <math.h>

static inline float kl_vecn_mag(const float* vs, int32_t dims) {
  float s = 0.0f;
  for (int32_t i = 0; i < dims; i++) {
    s += vs[i] * vs[i];
  }
  return sqrtf(s);
}

static inline float kl_vecn_dot(const float* a, const float* b, int32_t dims) {
  float s = 0.0f;
  for (int32_t i = 0; i < dims; i++) {
    s += a[i] * b[i];
  }
  return s;
}

static inline void kl_vecn_unit(const float* vs, float* out, int32_t dims) {
  float mag = kl_vecn_mag(vs, dims);
  if (mag <= 0.0f) return;

  float inv_mag = 1.0f / mag;

  for (int32_t i = 0; i < dims; i++) {
    out[i] = vs[i] * inv_mag;
  }
}

static inline void kl_vecn_scale(const float* vs, float* out, float s, int32_t dims) {
  for (int32_t i = 0; i < dims; i++) {
    out[i] = vs[i] * s;
  }
}


#define KL_VECN_OP_VV(NAME, OP)\
  static inline void kl_vecn_##NAME(const float* a, const float* b, float* out, int32_t dims) {\
    for (int32_t i = 0; i < dims; i++) {\
      out[i] = a[i] OP b[i];\
    }\
  }


KL_VECN_OP_VV(add, +);
KL_VECN_OP_VV(sub, -);
KL_VECN_OP_VV(mul, *);
KL_VECN_OP_VV(div, /);

#endif
