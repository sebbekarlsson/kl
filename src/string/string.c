#include <kl/string/string.h>
#include <kl/string/scanner.h>
#include <kl/macros.h>
#include <ctype.h>
#include <assert.h>
#include <stdarg.h>
#include <stdio.h>

bool kl_str_const_eq(KL_StrConst a, KL_StrConst b) {
  if (a.length != b.length) return false;
  if (a.value == b.value) return true;
  if (a.length <= 0) return true;
  if (a.value == 0 || b.value == 0) return false;
  return memcmp(a.value, b.value, a.length) == 0;
}

bool kl_str_const_starts_with(KL_StrConst str, KL_StrConst sub) {
  if (str.value == 0 || str.length <= 0) return false;
  if (sub.value == 0 || sub.length <= 0) return false;

  if (sub.length > str.length) return false;

  return memcmp(str.value, sub.value, sub.length) == 0;
}

bool kl_str_const_ends_with(KL_StrConst str, KL_StrConst sub) {
  if (str.value == 0 || str.length <= 0) return false;
  if (sub.value == 0 || sub.length <= 0) return false;

  int64_t start = str.length - sub.length;
  if (start < 0) return false;

  return memcmp(&str.value[start], sub.value, sub.length) == 0;
}

bool kl_str_const_includes(KL_StrConst str, KL_StrConst sub) {
  if (str.value == 0 || str.length <= 0) return false;
  if (sub.value == 0 || sub.length <= 0) return false;
  if (sub.length > str.length) return false;

  for (int64_t offset = 0; offset < str.length; offset++) {
    KL_StrConst view = kl_str_const_slice(str, offset, sub.length);
    if (kl_str_const_eq(view, sub)) return true;
  }

  return false;
}

int64_t kl_str_const_first_index_of(KL_StrConst str, KL_StrConst sub) {
  if (str.length <= 0 || str.value == 0) return -1;
  if (sub.length <= 0 || sub.value == 0) return -1;
  KL_StrScanner scanner = {0};
  if (!kl_str_scanner_init(&scanner, str)) return -1;

  while (kl_str_scanner_has_more(&scanner)) {
    if (kl_str_scanner_has(&scanner, sub)) {
      return scanner.i - 1;
    }
    kl_str_scanner_advance(&scanner, 1);
  }

  return -1;
}

bool kl_str_const_is_integer(KL_StrConst str) {
  if (str.value == 0 || str.length <= 0) return false;

  int64_t start = 0;
  if (str.value[0] == '-' || str.value[0] == '+') {
    if (str.length == 1) return false; // a lone sign is not an integer
    start = 1;
  }

  for (int64_t i = start; i < str.length; i++) {
    if (!isdigit((unsigned char)str.value[i])) return false;
  }

  return true;
}

bool kl_str_const_is_numeric(KL_StrConst str) {
  if (str.value == 0 || str.length <= 0) return false;

  int64_t start = 0;
  if (str.value[0] == '-' || str.value[0] == '+') {
    if (str.length == 1) return false; // a lone sign is not numeric
    start = 1;
  }

  bool seen_dot = false;
  bool seen_digit = false;

  for (int64_t i = start; i < str.length; i++) {
    char c = str.value[i];
    if (c == '.') {
      if (seen_dot) return false; // at most one decimal point
      seen_dot = true;
    } else if (isdigit((unsigned char)c)) {
      seen_digit = true;
    } else {
      return false;
    }
  }

  return seen_digit; // reject "", "-", ".", "-." etc.
}

KL_StrConst kl_str_const_slice(KL_StrConst str, int64_t offset, int64_t length) {
  if (str.value == 0 || str.length <= 0 || length <= 0 || offset >= str.length) return (KL_StrConst){0};

  offset = KL_MAX(0, offset);

  return (KL_StrConst){
    .value = &str.value[offset],
    .length = KL_CLAMP(length, 0, str.length - offset)
  };
}

KL_StrConst kl_str_const_remove_prefix(KL_StrConst str, KL_StrConst sub) {
  if (str.value == 0 || str.length <= 0) return str;
  if (sub.value == 0 || sub.length <= 0) return str;

  if (!kl_str_const_starts_with(str, sub)) return str;

  return (KL_StrConst){
    .value = &str.value[sub.length],
    .length = KL_MAX(0, str.length - sub.length)
  };
}

KL_StrConst kl_str_const_remove_numeric_prefix(KL_StrConst str) {
  if (str.value == 0 || str.length <= 0) return str;

  KL_StrScanner scanner = {0};
  if (!kl_str_scanner_init(&scanner, str)) return str;

  while (kl_str_scanner_has_more(&scanner) && isdigit(scanner.c)) {
    kl_str_scanner_advance(&scanner, 1);
    if (scanner.c == '.') {
      kl_str_scanner_advance(&scanner, 1);
    }
  }

  if (scanner.i <= 0) return str;
  if (scanner.i >= str.length) return (KL_StrConst){0};

  return kl_str_const_slice(str, scanner.i - 1, str.length - (scanner.i - 1));
}

KL_StrConst kl_str_const_remove_numeric_suffix(KL_StrConst str) {
  if (str.value == 0 || str.length <= 0) return str;

  int64_t i = str.length - 1;
  while (i >= 0 && isdigit(str.value[i])) {
    i--;
    if (i >= 0 && str.value[i] == '.') {
      i--;
    }
  }

  if (i >= str.length - 1) return str;
  if (i < 0) return (KL_StrConst){0};

  return kl_str_const_slice(str, 0, i + 1);
}

KL_StrConst kl_str_const_remove_suffix(KL_StrConst str, KL_StrConst sub) {
  if (str.value == 0 || str.length <= 0) return str;
  if (sub.value == 0 || sub.length <= 0) return str;

  if (!kl_str_const_ends_with(str, sub)) return str;

  return (KL_StrConst){
    .value = str.value,
    .length = KL_MAX(0, str.length - sub.length)
  };
}

KL_StrConst kl_str_const_strdup(KL_StrConst str, KL_Allocator* allocator) {
  assert(allocator != 0);
  assert(allocator->initialized);
  if (str.length <= 0 || str.value == 0) return (KL_StrConst){0};

  char* copy = kl_allocator_strdup(allocator, str.value, str.length);
  assert(copy != 0);

  return (KL_StrConst){
    .value = copy,
    .length = str.length
  };
}

KL_StrConst kl_str_const_concat(KL_StrConst a, KL_StrConst b, KL_Allocator* allocator) {
  assert(allocator != 0);
  if (a.length <= 0 && b.length <= 0) return KL_STR_CONST_NULL();
  if (a.length <= 0 && b.length > 0) return b;
  if (a.length > 0 && b.length <= 0) return a;

  assert(a.value != 0);
  assert(b.value != 0);

  KL_StrBuffer buff = {0};
  buff.allocator = allocator;
  buff.bump_size = a.length + b.length + 1;
  kl_str_buffer_append(&buff, a);
  kl_str_buffer_append(&buff, b);

  return KL_STR_BUFFER_TO_STR_CONST(buff);
}


KL_StrConst kl_str_const_join(KL_StrConst a, KL_StrConst b, KL_StrConst between, KL_Allocator* allocator) {
  if (between.length <= 0 || between.value == 0) return kl_str_const_concat(a, b, allocator);

  KL_StrBuffer buff = {0};
  buff.allocator = allocator;
  buff.bump_size = a.length + b.length + between.length + 1;
  
  if (a.length > 0 && a.value != 0) {
    kl_str_buffer_append(&buff, a);
  }

  kl_str_buffer_append(&buff, between);

  if (b.length > 0 && b.value != 0) {
    kl_str_buffer_append(&buff, b);
  }
  
  return KL_STR_BUFFER_TO_STR_CONST(buff);
}

KL_StrConst kl_str_const_to_lower(KL_StrConst str, KL_Allocator* allocator) {
  if (str.length <= 0 || str.value == 0) return str;

  char* copy = kl_allocator_strdup(allocator, str.value, str.length);
  assert(copy != 0);
  copy[0] = tolower(copy[0]);

  return (KL_StrConst){ .value = copy, .length = str.length };
}

KL_StrConst kl_str_const_replace(KL_StrConst str, KL_StrConst search, KL_StrConst repl, bool all, KL_StrBuffer* buffer) {
  assert(buffer != 0);

  // The buffer is scratch space owned by this call: start fresh and build the
  // whole result into it, then hand the buffer back as the result.
  kl_str_buffer_clear(buffer);

  if (str.length <= 0 || str.value == 0) {
    return str;
  }

  if (str.length < search.length) {
    return str;
  }

  KL_StrConst rest = str;
  bool replaced = false;

  while (rest.length >= search.length) {
    int64_t idx = kl_str_const_first_index_of(rest, search);
    if (idx < 0) break;

    assert(idx >= 0);
    assert(idx < rest.length);

    KL_StrConst left = kl_str_const_slice(rest, 0, idx);
    kl_str_buffer_append(buffer, left);
    kl_str_buffer_append(buffer, repl);

    int64_t consumed = idx + search.length;
    rest = kl_str_const_slice(rest, consumed, rest.length - consumed);
    replaced = true;

    if (!all) break;
  }

  // Append whatever remains: the tail after the last match, or the whole
  // string when nothing matched at all.
  kl_str_buffer_append(buffer, replaced ? rest : str);

  return (KL_StrConst){ .value = buffer->data, .length = buffer->length };
}

bool kl_str_buffer_fit(KL_StrBuffer* buff, int64_t extra) {
  if (buff->allocator == 0) return false;
  if (extra <= 0) return false;

  int64_t avail = buff->capacity - buff->length;

  if (avail >= extra) return true;
  
  int64_t new_size = buff->capacity + KL_MAX(buff->bump_size, extra);

  buff->data = (char*)kl_allocator_realloc(buff->allocator, new_size * sizeof(char), &buff->allocation);
  assert(buff->data != 0);

  buff->capacity = new_size;

  return true;
}

int64_t kl_str_buffer_avail(const KL_StrBuffer* buff) {
  if (buff->data == 0) return 0;
  return buff->capacity - buff->length;
}

bool kl_str_buffer_append_char(KL_StrBuffer* buff, char c) {
  kl_str_buffer_fit(buff, 2);
  
  int64_t avail = kl_str_buffer_avail(buff);
  if (avail <= 0) return false;
  buff->data[buff->length++] = c;
  /* Keep the buffer NUL-terminated. `capacity` excludes the terminator slot,
     so data[length] (length <= capacity) is always in bounds. */
  buff->data[buff->length] = '\0';
  return true;
}

bool kl_str_buffer_append(KL_StrBuffer* buff, KL_StrConst content) {
  if (content.length <= 0 || content.value == 0) return false;

  /* Reserve room for the whole span up front (+1 for the NUL terminator) and
     copy it in one shot rather than paying per-char fit/append overhead.
     fit() is best-effort: it no-ops without an allocator, so (like
     append_char) we rely on the available capacity check below. */
  kl_str_buffer_fit(buff, content.length + 1);
  if (kl_str_buffer_avail(buff) < content.length) return false;

  memcpy(&buff->data[buff->length], content.value, content.length);
  buff->length += content.length;
  buff->data[buff->length] = '\0';

  return true;
}

bool kl_str_buffer_append_fmt(KL_StrBuffer* buff, const char* fmt, ...) {
  if (fmt == 0) return false;

  /* First pass: measure the formatted length without writing anything.
     vsnprintf returns the number of chars (excluding the NUL) that *would*
     have been written. We copy the va_list because the first pass consumes
     it and we need a fresh one for the real write. */
  va_list args;
  va_start(args, fmt);

  va_list measure;
  va_copy(measure, args);
  int needed = vsnprintf(0, 0, fmt, measure);
  va_end(measure);

  if (needed < 0) {
    va_end(args);
    return false;
  }

  if (needed == 0) {
    va_end(args);
    return true; // nothing to append
  }

  /* Reserve room for the content plus the NUL terminator. With an allocator
     fit() always grows to satisfy the request, so the guard below can only
     trip for an allocator-less (e.g. stack) buffer that has run out of room;
     there it protects against an overflow instead of growing. */
  kl_str_buffer_fit(buff, needed + 1);
  if (kl_str_buffer_avail(buff) < needed) {
    va_end(args);
    return false;
  }

  /* capacity excludes the terminator slot, so data[capacity] is always a
     valid byte: avail + 1 bytes are writable starting at data[length]. */
  int written = vsnprintf(&buff->data[buff->length], needed + 1, fmt, args);
  va_end(args);

  if (written < 0) return false;

  buff->length += written;
  return true;
}

KL_StrConst kl_str_buffer_set(KL_StrBuffer* buff, KL_StrConst value) {
  if (value.length <= 0 || value.value == 0) {
    kl_str_buffer_clear(buff);
    return (KL_StrConst){0};
  }

  kl_str_buffer_clear(buff);
  
  if (!kl_str_buffer_append(buff, value)) return (KL_StrConst){0};

  return (KL_StrConst){
    .value = buff->data,
    .length = buff->length
  };
}

bool kl_str_buffer_repeat_char(KL_StrBuffer* buff, char c, int64_t count) {
  if (buff->data == 0 || buff->capacity <= 0 || c == 0 || count <= 0) return false;
  for (int64_t i = 0; i < count; i++) {
    if (!kl_str_buffer_append_char(buff, c)) return false;
  }
  return true;
}

KL_StrBuffer* kl_str_buffer_clear(KL_StrBuffer* buff) {
  if (buff->data == 0 || buff->capacity <= 0) return buff;
  /* Only clear the region that was actually written since the last clear,
     not the whole capacity. The buffer can be backed by a multi-megabyte
     allocation (e.g. the JSON tokenizer sizes it to the input length), so
     memset-ing the full capacity on every clear is catastrophically slow. */
  if (buff->length > 0) memset(&buff->data[0], 0, buff->length);
  buff->data[0] = '\0';
  buff->length = 0;

  return buff;
}

KL_StrBuffer* kl_str_buffer_stringify_blob(KL_StrBuffer* buff, KL_Blob blob, bool null_terminate, bool json_escape) {
  if (blob.size <= 0 || blob.value == 0) {
    kl_str_buffer_append(buff, KL_STR_CONST_LIT("(null)"));
    return buff;
  }

  for (int64_t i = 0; i < blob.size; i++) {
    uint8_t x = blob.value[i];

    if (x == 0 && null_terminate) {
      break;
    }

    if (json_escape) {
      /* Emit a string body that is valid inside a JSON "..." literal. Bytes
         >= 0x20 (including UTF-8 continuation bytes) pass through verbatim;
         control chars use their short escape or a \u00XX hex escape. */
      switch (x) {
        case '"':  kl_str_buffer_append(buff, KL_STR_CONST_LIT("\\\"")); continue;
        case '\\': kl_str_buffer_append(buff, KL_STR_CONST_LIT("\\\\")); continue;
        case '\b': kl_str_buffer_append(buff, KL_STR_CONST_LIT("\\b"));  continue;
        case '\f': kl_str_buffer_append(buff, KL_STR_CONST_LIT("\\f"));  continue;
        case '\n': kl_str_buffer_append(buff, KL_STR_CONST_LIT("\\n"));  continue;
        case '\r': kl_str_buffer_append(buff, KL_STR_CONST_LIT("\\r"));  continue;
        case '\t': kl_str_buffer_append(buff, KL_STR_CONST_LIT("\\t"));  continue;
        default: break;
      }
      if (x < 0x20) {
        kl_str_buffer_append_fmt(buff, "\\u%04x", x);
        continue;
      }
      kl_str_buffer_append_char(buff, (char)x);
      continue;
    }

    if (isprint((char)x)) {
      kl_str_buffer_append_char(buff, (char)x);
      continue;
    }

    kl_str_buffer_append_fmt(buff, "%x", x);
  }

  return buff;
}
