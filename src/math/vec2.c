#include <kl/math/vec2.h>
#include <kl/math/vecn.h>
#include <kl/math/macros.h>
#include <kl/common/number.h>

#define DIMS 2

float kl_vec2_mag(KL_Vec2 a) {
  return kl_vecn_mag(a.items, DIMS);
}

float kl_vec2_dot(KL_Vec2 a, KL_Vec2 b) {
  return kl_vecn_dot(a.items, b.items, DIMS);
}

KL_Vec2 kl_vec2_unit(KL_Vec2 a) {
  KL_Vec2 r = kl_vec21(0.0f);
  kl_vecn_unit(a.items, r.items, DIMS);
  return r;
}

KL_Vec2 kl_vec2_scale(KL_Vec2 a, float s) {
  KL_Vec2 r = kl_vec21(0.0f);
  kl_vecn_scale(a.items, r.items, s, DIMS);
  return r;
}

KL_Vec2 kl_vec2_add(KL_Vec2 a, KL_Vec2 b) {
  KL_Vec2 r = kl_vec21(0.0f);
  kl_vecn_add(a.items, b.items, r.items, DIMS);
  return r;
}

KL_Vec2 kl_vec2_sub(KL_Vec2 a, KL_Vec2 b) {
  KL_Vec2 r = kl_vec21(0.0f);
  kl_vecn_sub(a.items, b.items, r.items, DIMS);
  return r;
}

KL_Vec2 kl_vec2_mul(KL_Vec2 a, KL_Vec2 b) {
  KL_Vec2 r = kl_vec21(0.0f);
  kl_vecn_mul(a.items, b.items, r.items, DIMS);
  return r;
}

KL_Vec2 kl_vec2_div(KL_Vec2 a, KL_Vec2 b) {
  KL_Vec2 r = kl_vec21(0.0f);
  kl_vecn_div(a.items, b.items, r.items, DIMS);
  return r;
}

KL_Vec2 kl_vec2_lerp(KL_Vec2 a, KL_Vec2 b, float t) {
  return kl_vec2(
    kl_lerp_f32(a.x, b.x, t),
    kl_lerp_f32(a.y, b.y, t)
  );
}
