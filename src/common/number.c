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
