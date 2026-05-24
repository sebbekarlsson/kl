#include <kl/common/number.h>
#include <math.h>

bool kl_is_unsafe_f32(float x) {
  return isnanf(x) || isinff(x);
}

bool kl_is_safe_f32(float x) {
  return !kl_is_unsafe_f32(x);
}

float kl_clamp_f32(float v, float min, float max) {
  return fminf(fmaxf(v, min), max);
}

float kl_lerp_f32(float from, float to, float t) {
  return (1.0f - t) * from + t * to;
}

float kl_smoothstep_f32(float e0, float e1, float x) {
  float t = kl_clamp_f32((x - e0) / (e1 - e0), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}
