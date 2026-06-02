#ifndef KL_HASH_H
#define KL_HASH_H

#include <stdint.h>


uint32_t kl_hash_uint32(uint32_t x);

uint64_t kl_hash_uint64(uint64_t x);

uint32_t kl_hash_uint32_ptr(uint32_t* x);

uint64_t kl_hash_uint64_ptr(uint64_t* x);

float kl_hash_uint32_f32(uint32_t x);

double kl_hash_uint64_f64(uint64_t x);

float kl_hash_uint32_f32_ptr(uint32_t* x);

double kl_hash_uint64_f64_ptr(uint64_t* x);

float kl_hash_f32(float x);

double kl_hash_f64(double x);

////////////////////////////////

float kl_hash_uint32_next_f32(uint32_t* seed, float min, float max);

int32_t kl_hash_uint32_next_int32(uint32_t* seed, int32_t min, int32_t max);

#endif
