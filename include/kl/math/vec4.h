#ifndef KL_VEC4_H
#define KL_VEC4_H

typedef union {
  float items[4];
  
  struct {
    float x;
    float y;
    float z;
    float w;
  };
} KL_Vec4;

float kl_vec4_mag(KL_Vec4 a);

float kl_vec4_dot(KL_Vec4 a, KL_Vec4 b);

KL_Vec4 kl_vec4_unit(KL_Vec4 a);

KL_Vec4 kl_vec4_scale(KL_Vec4 a, float s);

KL_Vec4 kl_vec4_add(KL_Vec4 a, KL_Vec4 b);

KL_Vec4 kl_vec4_sub(KL_Vec4 a, KL_Vec4 b);

KL_Vec4 kl_vec4_mul(KL_Vec4 a, KL_Vec4 b);

KL_Vec4 kl_vec4_div(KL_Vec4 a, KL_Vec4 b);

KL_Vec4 kl_vec4_lerp(KL_Vec4 a, KL_Vec4 b, float t);

#endif
