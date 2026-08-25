#ifndef KL_NUMBER_H
#define KL_NUMBER_H

#include <stdbool.h>
#include <stdint.h>
#include <math.h>


#define KL_PI 3.141592653589793
#define KL_TAU 6.283185307179586

typedef float (*KL_F32Fn)(float x);

uint32_t kl_f32_to_uint32(float x);

uint64_t kl_f64_to_uint64(double x);

int64_t kl_amod_i64(int64_t x, int64_t d);

bool kl_is_unsafe_f32(float x);

bool kl_is_safe_f32(float x);

float kl_clamp_f32(float v, float min, float max);

double kl_clamp_f64(double v, double min, double max);

float kl_lerp_f32(float from, float to, float t);

double kl_lerp_f64(double from, double to, double t);

float kl_smoothstep_f32(float e0, float e1, float x);

double kl_smoothstep_f64(double e0, double e1, double x);

float kl_fract_f32(float x);

double kl_fract_f64(double x);

/**
* @brief smooth "less than"
**/
float kl_slt_f32(float a, float b, float s);

double kl_slt_f64(double a, double b, double s);

/**
* @brief logistic sigmoid, 1 / (1 + e^-x)
**/
float kl_sigmoid_f32(float x);

double kl_sigmoid_f64(double x);

#endif
