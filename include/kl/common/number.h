#ifndef KL_NUMBER_H
#define KL_NUMBER_H

#include <stdbool.h>

bool kl_is_unsafe_f32(float x);

bool kl_is_safe_f32(float x);

float kl_clamp_f32(float v, float min, float max);

float kl_smoothstep_f32(float e0, float e1, float x);

#endif
