#include <kl/noise/noise.h>
#include <kl/hash/hash.h>
#include <kl/common/number.h>
#include <kl/math/macros.h>
#include <math.h>


static inline float hash2(KL_Vec2 p, uint32_t seed) {
  uint32_t x = kl_f32_to_uint32(p.x);
  uint32_t y = kl_f32_to_uint32(p.y);
  uint32_t h = kl_hash_uint32_n((uint32_t[]){ x, y, seed }, 3);
  return (h >> 8) * (1.0f / (UINT32_C(1) << 24));
}

// quintic fade: 6t^5 - 15t^4 + 10t^3 (zero 1st and 2nd derivatives at 0 and 1)
static inline float fade(float t) {
  return t * t * t * (t * (t * 6.0f - 15.0f) + 10.0f);
}

// pseudo-random unit gradient at a lattice point
static inline KL_Vec2 grad2(KL_Vec2 p, uint32_t seed) {
  float a = hash2(p, seed) * (float)KL_TAU;
  return kl_vec2(cosf(a), sinf(a));
}

// radial contribution of one simplex corner: (r2_max - |d|^2)^4 * (grad . d),
// where d is the vector from the corner to the sample point.
static inline float simplex_corner(KL_Vec2 corner, KL_Vec2 d, uint32_t seed) {
  float t = 0.5f - kl_vec2_dot(d, d);
  if (t < 0.0f) return 0.0f;
  t *= t;
  return t * t * kl_vec2_dot(grad2(corner, seed), d);
}

float kl_noise_value_2d(KL_Vec2 p, uint32_t seed) {
  KL_Vec2 id = kl_vec2(floorf(p.x), floorf(p.y));
  KL_Vec2 lv = kl_vec2(kl_fract_f32(p.x), kl_fract_f32(p.y));

  // smoothstep
  lv.x = lv.x * lv.x * (3.0f - 2.0f * lv.x);
  lv.y = lv.y * lv.y * (3.0f - 2.0f * lv.y);

  return kl_lerp_f32(
    kl_lerp_f32(hash2(kl_vec2_add(id, kl_vec2(0, 0)), seed), hash2(kl_vec2_add(id, kl_vec2(1, 0)), seed), lv.x),
    kl_lerp_f32(hash2(kl_vec2_add(id, kl_vec2(0, 1)), seed), hash2(kl_vec2_add(id, kl_vec2(1, 1)), seed), lv.x),
    lv.y
  );
}

float kl_noise_perlin_2d(KL_Vec2 p, uint32_t seed) {
  KL_Vec2 id = kl_vec2(floorf(p.x), floorf(p.y));
  KL_Vec2 lv = kl_vec2(kl_fract_f32(p.x), kl_fract_f32(p.y));

  KL_Vec2 u = kl_vec2(fade(lv.x), fade(lv.y));

  // corner offsets within the cell
  KL_Vec2 c00 = kl_vec2(0, 0);
  KL_Vec2 c10 = kl_vec2(1, 0);
  KL_Vec2 c01 = kl_vec2(0, 1);
  KL_Vec2 c11 = kl_vec2(1, 1);

  // dot(gradient at corner, vector from corner to sample point)
  float d00 = kl_vec2_dot(grad2(kl_vec2_add(id, c00), seed), kl_vec2_sub(lv, c00));
  float d10 = kl_vec2_dot(grad2(kl_vec2_add(id, c10), seed), kl_vec2_sub(lv, c10));
  float d01 = kl_vec2_dot(grad2(kl_vec2_add(id, c01), seed), kl_vec2_sub(lv, c01));
  float d11 = kl_vec2_dot(grad2(kl_vec2_add(id, c11), seed), kl_vec2_sub(lv, c11));

  float n = kl_lerp_f32(
    kl_lerp_f32(d00, d10, u.x),
    kl_lerp_f32(d01, d11, u.x),
    u.y
  );

  // 2D gradient noise is bounded by [-sqrt(2)/2, sqrt(2)/2]; scale to [-1, 1]
  return n * 1.41421356f;
}

float kl_noise_simplex_2d(KL_Vec2 p, uint32_t seed) {
  // skew/unskew factors for 2D
  const float F2 = 0.366025403f; // 0.5 * (sqrt(3) - 1)
  const float G2 = 0.211324865f; // (3 - sqrt(3)) / 6

  // skew the input space to find which simplex cell we are in
  float s = (p.x + p.y) * F2;
  KL_Vec2 ij = kl_vec2(floorf(p.x + s), floorf(p.y + s));

  // unskew the cell origin back to (x, y) space; d0 = offset from that origin
  float t = (ij.x + ij.y) * G2;
  KL_Vec2 origin = kl_vec2(ij.x - t, ij.y - t);
  KL_Vec2 d0 = kl_vec2_sub(p, origin);

  // pick the second corner: lower vs upper triangle of the cell
  KL_Vec2 o = (d0.x > d0.y) ? kl_vec2(1, 0) : kl_vec2(0, 1);

  // offsets to the middle and far corners in unskewed (x, y) space
  KL_Vec2 d1 = kl_vec2(d0.x - o.x + G2, d0.y - o.y + G2);
  KL_Vec2 d2 = kl_vec2(d0.x - 1.0f + 2.0f * G2, d0.y - 1.0f + 2.0f * G2);

  float n = simplex_corner(ij, d0, seed)
          + simplex_corner(kl_vec2_add(ij, o), d1, seed)
          + simplex_corner(kl_vec2_add(ij, kl_vec2(1, 1)), d2, seed);

  // scale the summed contributions into ~[-1, 1] (empirical for unit gradients;
  // raw max ~0.01008, so 99.0 keeps the result just inside the bound)
  return n * 99.0f;
}

// ------- fbm -------

float kl_noise_value_2d_fbm(KL_Vec2 p, uint32_t seed, int32_t octaves, float lacunarity, float persistence) {
  float sum = 0.0f;
  float amp = 1.0f;
  float freq = 1.0f;
  float max_amp = 0.0f;

  for (int i = 0; i < octaves; i++) {
    sum += amp * kl_noise_value_2d(kl_vec2_scale(p, freq), seed + (uint32_t)i);
    max_amp += amp;
    freq *= lacunarity;
    amp *= persistence;
  }

  return sum / max_amp;
}

float kl_noise_perlin_2d_fbm(KL_Vec2 p, uint32_t seed, int32_t octaves, float lacunarity, float persistence) {
  float sum = 0.0f;
  float amp = 1.0f;
  float freq = 1.0f;
  float max_amp = 0.0f;

  for (int i = 0; i < octaves; i++) {
    sum += amp * kl_noise_perlin_2d(kl_vec2_scale(p, freq), seed + (uint32_t)i);
    max_amp += amp;
    freq *= lacunarity;
    amp *= persistence;
  }

  return sum / max_amp;
}

float kl_noise_simplex_2d_fbm(KL_Vec2 p, uint32_t seed, int32_t octaves, float lacunarity, float persistence) {
  float sum = 0.0f;
  float amp = 1.0f;
  float freq = 1.0f;
  float max_amp = 0.0f;

  for (int i = 0; i < octaves; i++) {
    sum += amp * kl_noise_simplex_2d(kl_vec2_scale(p, freq), seed + (uint32_t)i);
    max_amp += amp;
    freq *= lacunarity;
    amp *= persistence;
  }

  return sum / max_amp;
}
