#ifndef KL_VEC2_H
#define KL_VEC2_H
#include <stdint.h>

typedef union {
  float items[2];
  
  struct {
    float x;
    float y;
  };
} KL_Vec2;

float kl_vec2_mag(KL_Vec2 a);

float kl_vec2_dot(KL_Vec2 a, KL_Vec2 b);

KL_Vec2 kl_vec2_unit(KL_Vec2 a);

KL_Vec2 kl_vec2_scale(KL_Vec2 a, float s);

KL_Vec2 kl_vec2_add(KL_Vec2 a, KL_Vec2 b);

KL_Vec2 kl_vec2_sub(KL_Vec2 a, KL_Vec2 b);

KL_Vec2 kl_vec2_mul(KL_Vec2 a, KL_Vec2 b);

KL_Vec2 kl_vec2_div(KL_Vec2 a, KL_Vec2 b);

KL_Vec2 kl_vec2_lerp(KL_Vec2 a, KL_Vec2 b, float t);

typedef union {
  int32_t items[2];
  
  struct {
    int32_t x;
    int32_t y;
  };
} KL_IVec2;

#endif
