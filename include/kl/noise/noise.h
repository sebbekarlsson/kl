#ifndef KL_NOISE_H
#define KL_NOISE_H

#include <stdint.h>

#include <kl/math/vec2.h>

// value noise
float kl_noise_value_2d(KL_Vec2 p, uint32_t seed);

// perlin noise
float kl_noise_perlin_2d(KL_Vec2 p, uint32_t seed);

// simplex noise
float kl_noise_simplex_2d(KL_Vec2 p, uint32_t seed);

// ------- fbm -------

float kl_noise_value_2d_fbm(KL_Vec2 p, uint32_t seed, int32_t octaves, float lacunarity, float persistence);

float kl_noise_perlin_2d_fbm(KL_Vec2 p, uint32_t seed, int32_t octaves, float lacunarity, float persistence);

float kl_noise_simplex_2d_fbm(KL_Vec2 p, uint32_t seed, int32_t octaves, float lacunarity, float persistence);

#endif
