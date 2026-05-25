#include <kl/hash/hash.h>
#include <kl/common/number.h>

uint32_t kl_hash_uint32(uint32_t x) {
  x ^= x >> 16;
  x *= 0x85ebca6bU;
  x ^= x >> 13;
  x *= 0xc2b2ae35U;
  x ^= x >> 16;
  return x;
}

uint64_t kl_hash_uint64(uint64_t x) {
  x ^= x >> 30;
  x *= 0xbf58476d1ce4e5b9ULL;
  x ^= x >> 27;
  x *= 0x94d049bb133111ebULL;
  x ^= x >> 31;
  return x;
}

uint32_t kl_hash_uint32_ptr(uint32_t* x) {
  *x += 0x9e3779b9U;
  return kl_hash_uint32(*x);
}

uint64_t kl_hash_uint64_ptr(uint64_t* x) {
  *x += 0x9e3779b97f4a7c15ULL;
  return kl_hash_uint64(*x);
}

float kl_hash_uint32_f32(uint32_t x) {
  return (kl_hash_uint32(x) >> 8) * (1.0f / (UINT32_C(1) << 24));
}

double kl_hash_uint64_f64(uint64_t x) {
  return (kl_hash_uint64(x) >> 11) * (1.0 / (UINT64_C(1) << 53));
}

float kl_hash_uint32_f32_ptr(uint32_t* x) {
  return (kl_hash_uint32_ptr(x) >> 8) * (1.0f / (UINT32_C(1) << 24));
}

double kl_hash_uint64_f64_ptr(uint64_t* x) {
  return (kl_hash_uint64_ptr(x) >> 11) * (1.0 / (UINT64_C(1) << 53));
}

float kl_hash_f32(float x) {
  return kl_hash_uint32_f32(kl_f32_to_uint32(x));
}

double kl_hash_f64(double x) {
  return kl_hash_uint64_f64(kl_f64_to_uint64(x));
}
