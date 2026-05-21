#include <kl/string/scanner.h>
#include <kl/macros.h>
#include <assert.h>
#include <ctype.h>
#include <stddef.h>


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

bool kl_str_scanner_advance(KL_StrScanner* scanner, int64_t steps) {
  assert(scanner->initialized);
  if (steps <= 0) return false;
  if (!kl_str_scanner_has_more(scanner)) return false;

  for (int64_t i = 0; i < steps && kl_str_scanner_has_more(scanner); i++) {
    scanner->c = scanner->str.value[scanner->i++];
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
  if (start < 0 || end < 0 || start >= scanner->str.length || end >= scanner->str.length) return (KL_StrConst){0};

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
