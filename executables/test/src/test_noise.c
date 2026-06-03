#include <test/tests.h>

/* ===================== noise test helpers ===================== */

#define NOISE_SEED 1337u
#define NOISE_RANGE_TOL 0.02f   // slack on the [0,1] / [-1,1] contract bounds
#define NOISE_MEAN_TOL  0.05f   // how far the grid mean may stray from 0
#define NOISE_JUMP_TOL  0.05f   // largest tolerated step between adjacent samples
#define NOISE_EXACT_EPS 1e-6f   // float-equality slack for "identical" results

typedef float (*NoiseFn)(KL_Vec2, uint32_t);
typedef float (*FbmFn)(KL_Vec2, uint32_t, int32_t, float, float);

// Sweep a grid and report observed min/max/mean. The irrational-ish step keeps
// samples off the integer lattice so interior (interpolated) values are covered.
static void noise_grid_stats(NoiseFn fn, uint32_t seed, float* out_min, float* out_max, float* out_mean) {
  float mn = INFINITY, mx = -INFINITY;
  double sum = 0.0;
  long count = 0;
  for (int i = 0; i < 256; i++) {
    for (int j = 0; j < 256; j++) {
      float n = fn(kl_vec2(i * 0.0973f, j * 0.0911f), seed);
      if (n < mn) mn = n;
      if (n > mx) mx = n;
      sum += n;
      count++;
    }
  }
  *out_min = mn;
  *out_max = mx;
  *out_mean = (float)(sum / (double)count);
}

static void fbm_grid_stats(FbmFn fn, uint32_t seed, float* out_min, float* out_max) {
  float mn = INFINITY, mx = -INFINITY;
  for (int i = 0; i < 200; i++) {
    for (int j = 0; j < 200; j++) {
      float n = fn(kl_vec2(i * 0.0193f, j * 0.0217f), seed, 5, 2.0f, 0.5f);
      if (n < mn) mn = n;
      if (n > mx) mx = n;
    }
  }
  *out_min = mn;
  *out_max = mx;
}

// Largest jump between adjacent samples along one line through the field.
static float line_max_jump(NoiseFn fn, uint32_t seed, float ox, float oy, float dx, float dy, int steps) {
  float maxjump = 0.0f;
  float prev = fn(kl_vec2(ox, oy), seed);
  for (int k = 1; k <= steps; k++) {
    float n = fn(kl_vec2(ox + k * dx, oy + k * dy), seed);
    float d = fabsf(n - prev);
    if (d > maxjump) maxjump = d;
    prev = n;
  }
  return maxjump;
}

// Worst adjacent-sample jump along horizontal, vertical, and diagonal lines that
// each cross many cell boundaries. A seam is a jump that does not shrink with the
// step size; a continuous field stays far below NOISE_JUMP_TOL at this step.
static float noise_max_step_jump(NoiseFn fn, uint32_t seed) {
  const float h = 1e-3f;
  const int steps = 5000; // spans 5 units => crosses several cell boundaries
  float a = line_max_jump(fn, seed, 0.0f, 0.137f, h, 0.0f, steps);
  float b = line_max_jump(fn, seed, 0.137f, 0.0f, 0.0f, h, steps);
  float c = line_max_jump(fn, seed, 0.0f, 0.0f, h, h, steps);
  return fmaxf(fmaxf(a, b), c);
}

// Largest output difference between two seeds across a grid of off-lattice points.
static float noise_max_seed_diff(NoiseFn fn, uint32_t seed_a, uint32_t seed_b) {
  float maxd = 0.0f;
  for (int i = 0; i < 64; i++) {
    for (int j = 0; j < 64; j++) {
      KL_Vec2 p = kl_vec2(i * 0.0739f + 0.01f, j * 0.0853f + 0.01f);
      float d = fabsf(fn(p, seed_a) - fn(p, seed_b));
      if (d > maxd) maxd = d;
    }
  }
  return maxd;
}

/* ===================== value noise ===================== */

void test_noise_value_2d_deterministic() {
  KL_Vec2 p = kl_vec2(3.14159f, 2.71828f);
  float a = kl_noise_value_2d(p, NOISE_SEED);
  // an unrelated call in between must not perturb the result (no hidden state)
  (void)kl_noise_value_2d(kl_vec2(9.0f, 9.0f), NOISE_SEED + 5u);
  float b = kl_noise_value_2d(p, NOISE_SEED);
  KL_TEST_EXPECT_EQ_FLOAT(a, b, 0.0f);
}

void test_noise_value_2d_range() {
  float mn, mx, mean;
  noise_grid_stats(kl_noise_value_2d, NOISE_SEED, &mn, &mx, &mean);
  KL_TEST_ASSERT(mn >= -NOISE_RANGE_TOL, "value noise dipped below 0");
  KL_TEST_ASSERT(mx <= 1.0f + NOISE_RANGE_TOL, "value noise exceeded 1");
  // confirms it spans the range rather than collapsing toward a constant
  KL_TEST_ASSERT(mn < 0.25f && mx > 0.75f, "value noise does not span [0,1]");
}

void test_noise_value_2d_continuity() {
  float jump = noise_max_step_jump(kl_noise_value_2d, NOISE_SEED);
  KL_TEST_ASSERT(jump < NOISE_JUMP_TOL, "value noise has a seam/discontinuity");
}

void test_noise_value_2d_seed_sensitivity() {
  float d = noise_max_seed_diff(kl_noise_value_2d, 1u, 2u);
  KL_TEST_ASSERT(d > 0.01f, "value noise ignores the seed");
}

/* ===================== perlin noise ===================== */

void test_noise_perlin_2d_deterministic() {
  KL_Vec2 p = kl_vec2(3.14159f, 2.71828f);
  float a = kl_noise_perlin_2d(p, NOISE_SEED);
  (void)kl_noise_perlin_2d(kl_vec2(9.0f, 9.0f), NOISE_SEED + 5u);
  float b = kl_noise_perlin_2d(p, NOISE_SEED);
  KL_TEST_EXPECT_EQ_FLOAT(a, b, 0.0f);
}

void test_noise_perlin_2d_range() {
  float mn, mx, mean;
  noise_grid_stats(kl_noise_perlin_2d, NOISE_SEED, &mn, &mx, &mean);
  KL_TEST_ASSERT(mn >= -1.0f - NOISE_RANGE_TOL, "perlin noise fell below -1");
  KL_TEST_ASSERT(mx <= 1.0f + NOISE_RANGE_TOL, "perlin noise rose above 1");
  float maxabs = fmaxf(fabsf(mn), fabsf(mx));
  KL_TEST_ASSERT(maxabs > 0.5f, "perlin noise amplitude is implausibly small");
}

void test_noise_perlin_2d_zero_at_lattice() {
  // gradient noise is exactly 0 at integer lattice points: the offset vector to
  // the corner the sample sits on is zero, so every dot product is zero.
  for (int i = -4; i <= 4; i++) {
    for (int j = -4; j <= 4; j++) {
      float n = kl_noise_perlin_2d(kl_vec2((float)i, (float)j), NOISE_SEED);
      KL_TEST_EXPECT_EQ_FLOAT(n, 0.0f, 1e-5f);
    }
  }
}

void test_noise_perlin_2d_continuity() {
  float jump = noise_max_step_jump(kl_noise_perlin_2d, NOISE_SEED);
  KL_TEST_ASSERT(jump < NOISE_JUMP_TOL, "perlin noise has a seam/discontinuity");
}

void test_noise_perlin_2d_zero_mean() {
  float mn, mx, mean;
  noise_grid_stats(kl_noise_perlin_2d, NOISE_SEED, &mn, &mx, &mean);
  KL_TEST_ASSERT(fabsf(mean) < NOISE_MEAN_TOL, "perlin noise is not approximately zero-mean");
}

void test_noise_perlin_2d_seed_sensitivity() {
  float d = noise_max_seed_diff(kl_noise_perlin_2d, 1u, 2u);
  KL_TEST_ASSERT(d > 0.01f, "perlin noise ignores the seed");
}

/* ===================== simplex noise ===================== */

void test_noise_simplex_2d_deterministic() {
  KL_Vec2 p = kl_vec2(3.14159f, 2.71828f);
  float a = kl_noise_simplex_2d(p, NOISE_SEED);
  (void)kl_noise_simplex_2d(kl_vec2(9.0f, 9.0f), NOISE_SEED + 5u);
  float b = kl_noise_simplex_2d(p, NOISE_SEED);
  KL_TEST_EXPECT_EQ_FLOAT(a, b, 0.0f);
}

void test_noise_simplex_2d_range() {
  float mn, mx, mean;
  noise_grid_stats(kl_noise_simplex_2d, NOISE_SEED, &mn, &mx, &mean);
  KL_TEST_ASSERT(mn >= -1.0f - NOISE_RANGE_TOL, "simplex noise fell below -1");
  KL_TEST_ASSERT(mx <= 1.0f + NOISE_RANGE_TOL, "simplex noise rose above 1");
  float maxabs = fmaxf(fabsf(mn), fabsf(mx));
  KL_TEST_ASSERT(maxabs > 0.5f, "simplex noise amplitude is implausibly small");
}

void test_noise_simplex_2d_continuity() {
  float jump = noise_max_step_jump(kl_noise_simplex_2d, NOISE_SEED);
  KL_TEST_ASSERT(jump < NOISE_JUMP_TOL, "simplex noise has a seam/discontinuity");
}

void test_noise_simplex_2d_zero_mean() {
  float mn, mx, mean;
  noise_grid_stats(kl_noise_simplex_2d, NOISE_SEED, &mn, &mx, &mean);
  KL_TEST_ASSERT(fabsf(mean) < NOISE_MEAN_TOL, "simplex noise is not approximately zero-mean");
}

void test_noise_simplex_2d_seed_sensitivity() {
  float d = noise_max_seed_diff(kl_noise_simplex_2d, 1u, 2u);
  KL_TEST_ASSERT(d > 0.01f, "simplex noise ignores the seed");
}

/* ===================== fbm ===================== */

void test_noise_value_2d_fbm_single_octave_equals_base() {
  // one octave collapses fbm to the base function exactly (amp=1, freq=1, seed+0)
  KL_Vec2 pts[] = { kl_vec2(0.3f, 0.7f), kl_vec2(4.2f, 1.1f), kl_vec2(-2.5f, 3.3f) };
  for (int i = 0; i < 3; i++) {
    float base = kl_noise_value_2d(pts[i], NOISE_SEED);
    float fbm = kl_noise_value_2d_fbm(pts[i], NOISE_SEED, 1, 2.0f, 0.5f);
    KL_TEST_EXPECT_EQ_FLOAT(fbm, base, NOISE_EXACT_EPS);
  }
}

void test_noise_value_2d_fbm_range() {
  float mn, mx;
  fbm_grid_stats(kl_noise_value_2d_fbm, NOISE_SEED, &mn, &mx);
  KL_TEST_ASSERT(mn >= -NOISE_RANGE_TOL, "value fbm dipped below 0");
  KL_TEST_ASSERT(mx <= 1.0f + NOISE_RANGE_TOL, "value fbm exceeded 1");
}

void test_noise_perlin_2d_fbm_single_octave_equals_base() {
  KL_Vec2 pts[] = { kl_vec2(0.3f, 0.7f), kl_vec2(4.2f, 1.1f), kl_vec2(-2.5f, 3.3f) };
  for (int i = 0; i < 3; i++) {
    float base = kl_noise_perlin_2d(pts[i], NOISE_SEED);
    float fbm = kl_noise_perlin_2d_fbm(pts[i], NOISE_SEED, 1, 2.0f, 0.5f);
    KL_TEST_EXPECT_EQ_FLOAT(fbm, base, NOISE_EXACT_EPS);
  }
}

void test_noise_perlin_2d_fbm_range() {
  float mn, mx;
  fbm_grid_stats(kl_noise_perlin_2d_fbm, NOISE_SEED, &mn, &mx);
  KL_TEST_ASSERT(mn >= -1.0f - NOISE_RANGE_TOL, "perlin fbm fell below -1");
  KL_TEST_ASSERT(mx <= 1.0f + NOISE_RANGE_TOL, "perlin fbm rose above 1");
}

void test_noise_simplex_2d_fbm_single_octave_equals_base() {
  KL_Vec2 pts[] = { kl_vec2(0.3f, 0.7f), kl_vec2(4.2f, 1.1f), kl_vec2(-2.5f, 3.3f) };
  for (int i = 0; i < 3; i++) {
    float base = kl_noise_simplex_2d(pts[i], NOISE_SEED);
    float fbm = kl_noise_simplex_2d_fbm(pts[i], NOISE_SEED, 1, 2.0f, 0.5f);
    KL_TEST_EXPECT_EQ_FLOAT(fbm, base, NOISE_EXACT_EPS);
  }
}

void test_noise_simplex_2d_fbm_range() {
  float mn, mx;
  fbm_grid_stats(kl_noise_simplex_2d_fbm, NOISE_SEED, &mn, &mx);
  KL_TEST_ASSERT(mn >= -1.0f - NOISE_RANGE_TOL, "simplex fbm fell below -1");
  KL_TEST_ASSERT(mx <= 1.0f + NOISE_RANGE_TOL, "simplex fbm rose above 1");
}
