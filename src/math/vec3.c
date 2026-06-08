#include <kl/math/vec3.h>
#include <kl/math/vecn.h>
#include <kl/math/macros.h>
#include <kl/common/number.h>

#define DIMS 3

static KL_VecN_f32 as_vecn(float* items) {
  return (KL_VecN_f32){ .items = items, .length = DIMS, .allocator = 0 };
}

float kl_vec3_mag(KL_Vec3 a) {
  return kl_vecn_f32_mag(as_vecn(a.items));
}

float kl_vec3_dot(KL_Vec3 a, KL_Vec3 b) {
  return kl_vecn_f32_dot(as_vecn(a.items), as_vecn(b.items));
}

KL_Vec3 kl_vec3_unit(KL_Vec3 a) {
  KL_VecN_f32 v = as_vecn(a.items);
  kl_vecn_f32_unit_ip(&v);
  return a;
}

KL_Vec3 kl_vec3_scale(KL_Vec3 a, float s) {
  KL_VecN_f32 v = as_vecn(a.items);
  kl_vecn_f32_scale_ip(&v, s);
  return a;
}

KL_Vec3 kl_vec3_add(KL_Vec3 a, KL_Vec3 b) {
  KL_VecN_f32 v = as_vecn(a.items);
  kl_vecn_f32_add_ip(&v, as_vecn(b.items));
  return a;
}

KL_Vec3 kl_vec3_sub(KL_Vec3 a, KL_Vec3 b) {
  KL_VecN_f32 v = as_vecn(a.items);
  kl_vecn_f32_sub_ip(&v, as_vecn(b.items));
  return a;
}

KL_Vec3 kl_vec3_mul(KL_Vec3 a, KL_Vec3 b) {
  KL_VecN_f32 v = as_vecn(a.items);
  kl_vecn_f32_mul_ip(&v, as_vecn(b.items));
  return a;
}

KL_Vec3 kl_vec3_div(KL_Vec3 a, KL_Vec3 b) {
  KL_VecN_f32 v = as_vecn(a.items);
  kl_vecn_f32_div_ip(&v, as_vecn(b.items));
  return a;
}

KL_Vec3 kl_vec3_lerp(KL_Vec3 a, KL_Vec3 b, float t) {
  return kl_vec3(
    kl_lerp_f32(a.x, b.x, t),
    kl_lerp_f32(a.y, b.y, t),
    kl_lerp_f32(a.z, b.z, t)
  );
}
