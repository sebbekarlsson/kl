#include <kl/math/vec3.h>
#include <kl/math/vecn.h>
#include <kl/math/macros.h>
#include <kl/common/number.h>

#define DIMS 3

float kl_vec3_mag(KL_Vec3 a) {
  return kl_vecn_mag(a.items, DIMS);
}

float kl_vec3_dot(KL_Vec3 a, KL_Vec3 b) {
  return kl_vecn_dot(a.items, b.items, DIMS);
}

KL_Vec3 kl_vec3_unit(KL_Vec3 a) {
  KL_Vec3 r = kl_vec31(0.0f);
  kl_vecn_unit(a.items, r.items, DIMS);
  return r;
}

KL_Vec3 kl_vec3_scale(KL_Vec3 a, float s) {
  KL_Vec3 r = kl_vec31(0.0f);
  kl_vecn_scale(a.items, r.items, s, DIMS);
  return r;
}

KL_Vec3 kl_vec3_add(KL_Vec3 a, KL_Vec3 b) {
  KL_Vec3 r = kl_vec31(0.0f);
  kl_vecn_add(a.items, b.items, r.items, DIMS);
  return r;
}

KL_Vec3 kl_vec3_sub(KL_Vec3 a, KL_Vec3 b) {
  KL_Vec3 r = kl_vec31(0.0f);
  kl_vecn_sub(a.items, b.items, r.items, DIMS);
  return r;
}

KL_Vec3 kl_vec3_mul(KL_Vec3 a, KL_Vec3 b) {
  KL_Vec3 r = kl_vec31(0.0f);
  kl_vecn_mul(a.items, b.items, r.items, DIMS);
  return r;
}

KL_Vec3 kl_vec3_div(KL_Vec3 a, KL_Vec3 b) {
  KL_Vec3 r = kl_vec31(0.0f);
  kl_vecn_div(a.items, b.items, r.items, DIMS);
  return r;
}

KL_Vec3 kl_vec3_lerp(KL_Vec3 a, KL_Vec3 b, float t) {
  return kl_vec3(
    kl_lerp_f32(a.x, b.x, t),
    kl_lerp_f32(a.y, b.y, t),
    kl_lerp_f32(a.z, b.z, t)
  );
}
