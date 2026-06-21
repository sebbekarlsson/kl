#include <kl/string/scanner.h>
#include <kl/macros.h>
#include <assert.h>
#include <ctype.h>
#include <stddef.h>

static const KL_StrConst alpha_ish = KL_STR_CONST_LIT("abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ_");

bool kl_str_scanner_init(KL_StrScanner* scanner, KL_StrConst str) {
  if (scanner->initialized) return true;
  if (str.value == 0 || str.length <= 0) return false;
  scanner->str = str;
  scanner->i = 0;
  scanner->c = str.value[scanner->i++];
  scanner->initialized = true;
  return true;
}

bool kl_str_scanner_is_eof(KL_StrScanner* scanner) {
  assert(scanner->initialized);
  
  return scanner->c == 0 ||
         scanner->i >= scanner->str.length ||
         scanner->str.value == 0;
}

bool kl_str_scanner_has_more(KL_StrScanner* scanner) {
  assert(scanner->initialized);
  
  return scanner->c != 0 &&
        scanner->i <= scanner->str.length &&
        scanner->str.value != 0;
}

bool kl_str_scanner_eat_char(KL_StrScanner* scanner, char c) {
  assert(scanner->initialized);
  if (scanner->c != c) return false;
  kl_str_scanner_advance(scanner, 1);
  return true;
}

bool kl_str_scanner_eat_until_char(KL_StrScanner* scanner, char c, KL_StrBuffer* out) {
  assert(scanner->initialized);

  while (kl_str_scanner_has_more(scanner) && scanner->c != c) {
    if (!kl_str_buffer_append_char(out, scanner->c)) return false;
    kl_str_scanner_advance(scanner, 1);
  }

  return out->length > 0;
}

bool kl_str_scanner_eat_integer(KL_StrScanner* scanner, KL_StrBuffer* out) {
  assert(scanner->initialized);

  if (!isdigit(scanner->c)) return false;
  if (!kl_str_buffer_append_char(out, scanner->c)) return false;
  
  kl_str_scanner_advance(scanner, 1);
   
  while (kl_str_scanner_has_more(scanner) && isdigit(scanner->c)) {
    if (!kl_str_buffer_append_char(out, scanner->c)) return false;
    kl_str_scanner_advance(scanner, 1);
  }

  return out->length > 0;
}

bool kl_str_scanner_eat_alpha(KL_StrScanner* scanner, KL_StrBuffer* out) {
  assert(scanner->initialized);

  if (!isalpha(scanner->c)) return false;
  if (!kl_str_buffer_append_char(out, scanner->c)) return false;
  
  kl_str_scanner_advance(scanner, 1);
   
  while (kl_str_scanner_has_more(scanner) && isalpha(scanner->c)) {
    if (!kl_str_buffer_append_char(out, scanner->c)) return false;
    kl_str_scanner_advance(scanner, 1);
  }

  return out->length > 0;
}

bool kl_str_scanner_eat_any_of(KL_StrScanner* scanner, KL_StrConst chars, KL_StrBuffer* out) {
  assert(scanner->initialized);

  while (kl_str_scanner_has_any_of(scanner, chars)) {
    if (!kl_str_buffer_append_char(out, scanner->c)) return false;
    kl_str_scanner_advance(scanner, 1);
  }

  return out->length > 0;
}

bool kl_str_scanner_eat_identifier(KL_StrScanner* scanner, KL_StrBuffer* out) {
  assert(scanner->initialized);

  if (!kl_str_scanner_eat_any_of(scanner, alpha_ish, out)) return false;

  while (kl_str_scanner_has_more(scanner) && (isdigit(scanner->c) || kl_str_scanner_has_any_of(scanner, alpha_ish))) {
    if (!kl_str_buffer_append_char(out, scanner->c)) return false;
    kl_str_scanner_advance(scanner, 1);
  }

  return out->length > 0;
}

bool kl_str_scanner_advance(KL_StrScanner* scanner, int64_t steps) {
  assert(scanner->initialized);
  if (steps <= 0) return false;
  if (!kl_str_scanner_has_more(scanner)) return false;

  /* The scalar form reads value[i++] up to `steps` times, stopping early if it
     reads a NUL (current char becomes 0) or i would pass the end. Reproduce
     that exact result in one shot: clamp to the terminator slot, then let a
     single (SIMD) memchr honor any embedded NUL in the skipped span. This turns
     a per-char loop (with a has_more() call each iteration) into O(1) amortized. */
  const char* v = scanner->str.value;
  int64_t len = scanner->str.length;
  int64_t i = scanner->i;

  int64_t max_reads = (len + 1) - i;   // keeps i <= len + 1, matching the loop
  if (max_reads < 0) max_reads = 0;
  int64_t n = steps < max_reads ? steps : max_reads;

  if (n > 0) {
    const void* nul = memchr(&v[i], 0, (size_t)n);
    if (nul != 0) {
      scanner->i = (int64_t)((const char*)nul - v) + 1;
      scanner->c = 0;
    } else {
      scanner->i = i + n;
      scanner->c = v[scanner->i - 1];
    }
  }

  return kl_str_scanner_has_more(scanner);
}


static inline bool is_whitespace(char c) {
  return c == ' ' || c == '\r' || c == '\n' || c == '\t';
}

bool kl_str_scanner_skip_whitespace(KL_StrScanner* scanner) {
  assert(scanner->initialized);
  if (!kl_str_scanner_has_more(scanner)) return false;

  while (kl_str_scanner_has_more(scanner) && is_whitespace(scanner->c)) {
    kl_str_scanner_advance(scanner, 1);
  }
  
  return kl_str_scanner_has_more(scanner);
}

char kl_str_scanner_peek(KL_StrScanner* scanner, int64_t offset) {
  assert(scanner->initialized);
  assert(scanner->str.value != 0);
  assert(scanner->str.length > 0);
  int64_t idx = (scanner->i - 1) + offset;
  if (idx < 0 || idx >= scanner->str.length) return 0;
  return scanner->str.value[idx];
}

KL_StrConst kl_str_scanner_slice(KL_StrScanner* scanner, int64_t length) {
  assert(scanner->initialized);
  assert(scanner->str.value != 0);
  assert(scanner->str.length > 0);
  int64_t start = (scanner->i - 1);
  int64_t end = start + length;
  if (start < 0 || end < 0 || start >= scanner->str.length || end > scanner->str.length) return (KL_StrConst){0};

  return (KL_StrConst){
    .value = &scanner->str.value[start],
    .length = length
  };
}

bool kl_str_scanner_has(KL_StrScanner* scanner, KL_StrConst token) {
  assert(scanner->initialized);
  assert(scanner->str.value != 0);
  assert(scanner->str.length > 0);
  KL_StrConst slice = kl_str_scanner_slice(scanner, token.length);
  if (slice.value == 0 || slice.length <= 0) return false;
  assert(slice.value != 0);
  assert(slice.length == token.length);

  return memcmp(slice.value, token.value, token.length) == 0;
}

bool kl_str_scanner_has_icase(KL_StrScanner* scanner, KL_StrConst token) {
  assert(scanner->initialized);
  assert(scanner->str.value != 0);
  assert(scanner->str.length > 0);
  KL_StrConst slice = kl_str_scanner_slice(scanner, token.length);
  if (slice.value == 0 || slice.length <= 0) return false;
  assert(slice.value != 0);
  assert(slice.length == token.length);

  for (int64_t i = 0; i < token.length; i++) {
    if (tolower((unsigned char)slice.value[i]) != tolower((unsigned char)token.value[i])) return false;
  }

  return true;
}

bool kl_str_scanner_has_any_of(KL_StrScanner* scanner, KL_StrConst chars) {
  assert(scanner->initialized);
  assert(scanner->str.value != 0);
  assert(scanner->str.length > 0);
  if (chars.length <= 0 || chars.value == 0) return false;

  for (int64_t i = 0; i < chars.length; i++) {
    if (scanner->c == chars.value[i]) return true;
  }
  
  return false;
}

bool kl_str_scanner_has_identifier_start(KL_StrScanner* scanner) {
  return kl_str_scanner_has_any_of(scanner, alpha_ish);
}

bool kl_str_scanner_has_digit(KL_StrScanner* scanner) {
  return scanner->c != 0 && isdigit(scanner->c);
}

bool kl_str_scanner_has_alpha(KL_StrScanner* scanner) {
  return scanner->c !=0 && isalpha(scanner->c);
}

bool kl_str_scanner_has_alnum(KL_StrScanner* scanner) {
  return scanner->c != 0 && isalnum(scanner->c);
}
