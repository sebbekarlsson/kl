#include <kl/hash/hash.h>
#include <kl/common/number.h>
#include <kl/macros.h>
#include <string.h>
#include <stddef.h>
#include <ctype.h>

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

uint32_t kl_hash_uint32_n(uint32_t* items, int32_t length) {
  if (items == 0) return 0;

  uint32_t h = 0xc3e4be18U;          // arbitrary nonzero seed

  for (int32_t i = 0; i < length; i++) {
    h ^= items[i];
    h = kl_hash_uint32(h);           // full avalanche before folding the next item
  }

  return h;
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

////////////////////////////////

float kl_hash_uint32_next_f32(uint32_t* seed, float min, float max) {
  return kl_lerp_f32(min, max, kl_hash_uint32_f32_ptr(seed)); 
}

int32_t kl_hash_uint32_next_int32(uint32_t* seed, int32_t min, int32_t max) {
  /* number of integers in [min, max]; unsigned math avoids signed overflow
   * and is correct in two's complement even when min/max straddle zero. */
  uint32_t span = (uint32_t)max - (uint32_t)min + 1u;
  uint32_t r = kl_hash_uint32_ptr(seed);
  if (span == 0u) {
    /* min..max spans the entire int32 range; every value is valid. */
    return (int32_t)r;
  }
  return (int32_t)((uint32_t)min + (r % span));
}

bool kl_hash_uint32_next_bool(uint32_t* seed) {
  return kl_hash_uint32_next_f32(seed, 0.0f, 1.0f) < 0.5f;
}

int32_t kl_hash_uint32_next_index(uint32_t* seed, int32_t array_length) {
  if (array_length <= 0) return 0;
  return kl_hash_uint32_next_int32(seed, 0, array_length-1) % array_length;
}

char kl_hash_uint32_next_char_alpha(uint32_t* seed) {
  return (char)kl_hash_uint32_next_int32(seed, 97, 122);
}

char kl_hash_uint32_next_char_numeric(uint32_t* seed) {
  return (char)kl_hash_uint32_next_int32(seed, 48, 57);
}

char kl_hash_uint32_next_char_hex_alpha(uint32_t* seed) {
  return (char)kl_hash_uint32_next_int32(seed, 'a', 'f');
}

char kl_hash_uint32_next_char_hex_alpha_numeric(uint32_t* seed) {
  return kl_hash_uint32_next_bool(seed) ? kl_hash_uint32_next_char_hex_alpha(seed) : kl_hash_uint32_next_char_numeric(seed);
}

char kl_hash_uint32_next_char_hex(uint32_t* seed) {
  return "0123456789abcdef"[kl_hash_uint32_next_index(seed, 16)];
}

static const char KL_VOWELS[] = {
  'a', 'e', 'i', 'o', 'u', 'y'
};
static const size_t KL_VOWELS_LEN = KL_ARRAY_LEN(KL_VOWELS);


static const char KL_CONSONANTS[] = {
  'b', 'c', 'd', 'f', 'g', 'h', 'j', 'k', 'l', 'm', 'n', 'p', 'r', 'q', 's', 't', 'v', 'w', 'x', 'z'
};
static const size_t KL_CONSONANTS_LEN = KL_ARRAY_LEN(KL_CONSONANTS);

char kl_hash_uint32_next_char_vowel(uint32_t* seed) {
  return KL_VOWELS[kl_hash_uint32_next_index(seed, (int32_t)KL_VOWELS_LEN)];
}

char kl_hash_uint32_next_char_consonant(uint32_t* seed) {
  return KL_CONSONANTS[kl_hash_uint32_next_index(seed, (int32_t)KL_CONSONANTS_LEN)];
}

bool kl_hash_uint32_next_string_alpha(uint32_t* seed, int32_t length, KL_StrBuffer* out) {
  for (int32_t i = 0; i < length; i++) {
    if (!kl_str_buffer_append_char(out, kl_hash_uint32_next_char_alpha(seed))) {
      break;
    }
  }
  return out->length > 0 && out->data != 0;
}

bool kl_hash_uint32_next_string_name(uint32_t* seed, int32_t length, KL_StrBuffer* out) {
  for (int32_t i = 0; i < length; i++) {
    char c = ((i % 2) == 0) ? kl_hash_uint32_next_char_consonant(seed) : kl_hash_uint32_next_char_vowel(seed);
    if (i <= 0) c = toupper(c);
    if (!kl_str_buffer_append_char(out, c)) {
      break;
    }
  }
  return out->length > 0 && out->data != 0;
}

bool kl_hash_uint32_next_string_pattern(uint32_t* seed, KL_StrConst pattern, KL_StrBuffer* out) {

  static const KL_StrConst ys = KL_STR_CONST_LIT("89ab");
  
  for (int64_t i = 0; i < pattern.length; i++) {
    char f = pattern.value[i];


    if (f >= '0' && f <= '9') {
      kl_str_buffer_append_char(out, f);
      continue;
    }

    switch (f) {
      case 'x': kl_str_buffer_append_char(out, kl_hash_uint32_next_char_hex(seed)); break;
      case 'y': kl_str_buffer_append_char(out, ys.value[kl_hash_uint32_next_index(seed, ys.length)]); break;
      default: kl_str_buffer_append_char(out, f); break;
    }
  }

  return out->length > 0 && out->data != 0;
}


static const KL_StrConst KL_UUIDV4_FORMAT = KL_STR_CONST_LIT("xxxxxxxx-xxxx-4xxx-yxxx-xxxxxxxxxxxx");

bool kl_hash_uint32_next_string_uuidv4(uint32_t* seed, KL_StrBuffer* out) {
  return kl_hash_uint32_next_string_pattern(seed, KL_UUIDV4_FORMAT, out);
}
