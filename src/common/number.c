#include <kl/common/number.h>
#include <math.h>

uint32_t kl_f32_to_uint32(float x) {
  union {
    float f;
    uint32_t i;
  } p = { .f = x };
  return p.i;
}

uint64_t kl_f64_to_uint64(double x) {
  union {
    double f;
    uint64_t i;
  } p = { .f = x };
  return p.i;
}

int64_t kl_amod_i64(int64_t x, int64_t d) {
  return ((x % d) + d) % d;
}

bool kl_is_unsafe_f32(float x) {
  return isnanf(x) || isinff(x);
}

bool kl_is_safe_f32(float x) {
  return !kl_is_unsafe_f32(x);
}

float kl_clamp_f32(float v, float min, float max) {
  return fminf(fmaxf(v, min), max);
}

double kl_clamp_f64(double v, double min, double max) {
  return fmin(fmax(v, min), max);
}

float kl_lerp_f32(float from, float to, float t) {
  return (1.0f - t) * from + t * to;
}

double kl_lerp_f64(double from, double to, double t) {
  return (1.0 - t) * from + t * to;
}

float kl_smoothstep_f32(float e0, float e1, float x) {
  float t = kl_clamp_f32((x - e0) / (e1 - e0), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

double kl_smoothstep_f64(double e0, double e1, double x) {
  double t = kl_clamp_f64((x - e0) / (e1 - e0), 0.0f, 1.0f);
  return t * t * (3.0f - 2.0f * t);
}

float kl_fract_f32(float x) {
  return x - floorf(x);
}

double kl_fract_f64(double x) {
  return x - floor(x);
}

float kl_slt_f32(float a, float b, float s) {
  float h = kl_clamp_f32(0.5f + (0.5f * (b - a)) / s, 0.0f, 1.0f);
  return kl_lerp_f32(0.0f, 1.0f, h * h * (3.0f - 2.0f * h));
}

double kl_slt_f64(double a, double b, double s) {
  double h = kl_clamp_f64(0.5 + (0.5 * (b - a)) / s, 0.0, 1.0);
  return kl_lerp_f64(0.0, 1.0, h * h * (3.0 - 2.0 * h));
}

float kl_sigmoid_f32(float x) {
  return 1.0f / (1.0f + expf(-x));
}

double kl_sigmoid_f64(double x) {
  return 1.0 / (1.0 + exp(-x));
}
