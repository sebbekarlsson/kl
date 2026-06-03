#ifndef KL_VEC3_H
#define KL_VEC3_H

typedef union {
  float items[3];
  
  struct {
    float x;
    float y;
    float z;
  };
} KL_Vec3;

float kl_vec3_mag(KL_Vec3 a);

float kl_vec3_dot(KL_Vec3 a, KL_Vec3 b);

KL_Vec3 kl_vec3_unit(KL_Vec3 a);

KL_Vec3 kl_vec3_scale(KL_Vec3 a, float s);

KL_Vec3 kl_vec3_add(KL_Vec3 a, KL_Vec3 b);

KL_Vec3 kl_vec3_sub(KL_Vec3 a, KL_Vec3 b);

KL_Vec3 kl_vec3_mul(KL_Vec3 a, KL_Vec3 b);

KL_Vec3 kl_vec3_div(KL_Vec3 a, KL_Vec3 b);

KL_Vec3 kl_vec3_lerp(KL_Vec3 a, KL_Vec3 b, float t);

#endif
