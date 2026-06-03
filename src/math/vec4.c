#include <kl/math/vec4.h>
#include <kl/math/vecn.h>
#include <kl/math/macros.h>
#include <kl/common/number.h>

#define DIMS 4

float kl_vec4_mag(KL_Vec4 a) {
  return kl_vecn_mag(a.items, DIMS);
}

float kl_vec4_dot(KL_Vec4 a, KL_Vec4 b) {
  return kl_vecn_dot(a.items, b.items, DIMS);
}

KL_Vec4 kl_vec4_unit(KL_Vec4 a) {
  KL_Vec4 r = kl_vec41(0.0f);
  kl_vecn_unit(a.items, r.items, DIMS);
  return r;
}

KL_Vec4 kl_vec4_scale(KL_Vec4 a, float s) {
  KL_Vec4 r = kl_vec41(0.0f);
  kl_vecn_scale(a.items, r.items, s, DIMS);
  return r;
}

KL_Vec4 kl_vec4_add(KL_Vec4 a, KL_Vec4 b) {
  KL_Vec4 r = kl_vec41(0.0f);
  kl_vecn_add(a.items, b.items, r.items, DIMS);
  return r;
}

KL_Vec4 kl_vec4_sub(KL_Vec4 a, KL_Vec4 b) {
  KL_Vec4 r = kl_vec41(0.0f);
  kl_vecn_sub(a.items, b.items, r.items, DIMS);
  return r;
}

KL_Vec4 kl_vec4_mul(KL_Vec4 a, KL_Vec4 b) {
  KL_Vec4 r = kl_vec41(0.0f);
  kl_vecn_mul(a.items, b.items, r.items, DIMS);
  return r;
}

KL_Vec4 kl_vec4_div(KL_Vec4 a, KL_Vec4 b) {
  KL_Vec4 r = kl_vec41(0.0f);
  kl_vecn_div(a.items, b.items, r.items, DIMS);
  return r;
}

KL_Vec4 kl_vec4_lerp(KL_Vec4 a, KL_Vec4 b, float t) {
  return kl_vec4(
    kl_lerp_f32(a.x, b.x, t),
    kl_lerp_f32(a.y, b.y, t),
    kl_lerp_f32(a.z, b.z, t),
    kl_lerp_f32(a.w, b.w, t)
  );
}
