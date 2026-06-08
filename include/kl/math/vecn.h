#ifndef KL_VECN_H
#define KL_VECN_H

#include <stdint.h>
#include <math.h>

#include <kl/memory/allocator.h>

typedef struct {
  float *items;
  int64_t length;
  KL_Allocator* allocator;
} KL_VecN_f32;


KL_VecN_f32 kl_vecn_f32_create(const float *items, int64_t length, KL_Allocator* allocator);
KL_VecN_f32 kl_vecn_f32_copy(KL_VecN_f32 src);

float kl_vecn_f32_mag(KL_VecN_f32 a);
float kl_vecn_f32_dot(KL_VecN_f32 a, KL_VecN_f32 b);
float kl_vecn_f32_cosine_distance(KL_VecN_f32 a, KL_VecN_f32 b);


// *_ip = in-place (No allocations)

void kl_vecn_f32_unit_ip(KL_VecN_f32* a);
KL_VecN_f32 kl_vecn_f32_unit(KL_VecN_f32 a);
void kl_vecn_f32_scale_ip(KL_VecN_f32* a, float s);
KL_VecN_f32 kl_vecn_f32_scale(KL_VecN_f32 a, float s);

void kl_vecn_f32_add_ip(KL_VecN_f32* a, KL_VecN_f32 b);
KL_VecN_f32 kl_vecn_f32_add(KL_VecN_f32 a, KL_VecN_f32 b);
void kl_vecn_f32_sub_ip(KL_VecN_f32* a, KL_VecN_f32 b);
KL_VecN_f32 kl_vecn_f32_sub(KL_VecN_f32 a, KL_VecN_f32 b);
void kl_vecn_f32_div_ip(KL_VecN_f32* a, KL_VecN_f32 b);
KL_VecN_f32 kl_vecn_f32_div(KL_VecN_f32 a, KL_VecN_f32 b);
void kl_vecn_f32_mul_ip(KL_VecN_f32* a, KL_VecN_f32 b);
KL_VecN_f32 kl_vecn_f32_mul(KL_VecN_f32 a, KL_VecN_f32 b);

#endif
