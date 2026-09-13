#ifndef KL_HASH_H
#define KL_HASH_H

#include <stdint.h>
#include <stdbool.h>

#include <kl/string/string.h>

uint32_t kl_hash_uint32(uint32_t x);

uint64_t kl_hash_uint64(uint64_t x);

uint32_t kl_hash_uint32_n(uint32_t* items, int32_t length);

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

bool kl_hash_uint32_next_bool(uint32_t* seed);

int32_t kl_hash_uint32_next_index(uint32_t* seed, int32_t array_length);

char kl_hash_uint32_next_char_alpha(uint32_t* seed);

char kl_hash_uint32_next_char_numeric(uint32_t* seed);

char kl_hash_uint32_next_char_hex_alpha(uint32_t* seed);

char kl_hash_uint32_next_char_hex_alpha_numeric(uint32_t* seed);

char kl_hash_uint32_next_char_hex(uint32_t* seed);

char kl_hash_uint32_next_char_vowel(uint32_t* seed);

char kl_hash_uint32_next_char_consonant(uint32_t* seed);

bool kl_hash_uint32_next_string_alpha(uint32_t* seed, int32_t length, KL_StrBuffer* out);

bool kl_hash_uint32_next_string_name(uint32_t* seed, int32_t length, KL_StrBuffer* out);

bool kl_hash_uint32_next_string_pattern(uint32_t* seed, KL_StrConst pattern, KL_StrBuffer* out);

bool kl_hash_uint32_next_string_uuidv4(uint32_t* seed, KL_StrBuffer* out);

uint8_t* kl_hash_uint32_next_choice(uint32_t* seed, uint8_t* items, size_t item_size, int64_t item_count);

#endif
