#include <kl/string/string.h>
#include <kl/macros.h>
#include <ctype.h>
#include <assert.h>

bool kl_str_const_eq(KL_StrConst a, KL_StrConst b) {
  if (a.length != b.length) return false;
  if (a.value == 0 && b.value == 0) return true;
  if (a.length <= 0 || a.value == 0 || b.length <= 0 || b.value == 0) return false;
  return memcmp(a.value, b.value, b.length) == 0;
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

bool kl_str_const_is_integer(KL_StrConst str) {
  if (str.value == 0 || str.length <= 0) return false;

  for (int64_t i = 0; i < str.length; i++) {
    if (!isdigit(str.value[i])) return false;
  }
  
  return true;
}

KL_StrConst kl_str_const_slice(KL_StrConst str, int64_t offset, int64_t length) {
  if (str.value == 0 || str.length <= 0 || length <= 0 || offset < 0 || offset >= str.length) return (KL_StrConst){0};

  return (KL_StrConst){
    .value = &str.value[offset],
    .length = KL_CLAMP(length, 0, str.length)
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

KL_StrConst kl_str_const_remove_suffix(KL_StrConst str, KL_StrConst sub) {
  if (str.value == 0 || str.length <= 0) return str;
  if (sub.value == 0 || sub.length <= 0) return str;

  if (!kl_str_const_ends_with(str, sub)) return str;

  return (KL_StrConst){
    .value = str.value,
    .length = KL_MAX(0, str.length - sub.length)
  };
}


static inline bool kl_str_buffer_fit(KL_StrBuffer* buff, int64_t extra) {
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
  
  for (int64_t i = 0; i < content.length; i++) {
    if (!kl_str_buffer_append_char(buff, content.value[i])) {
      return false;
    }
  }

  return true;
}

bool kl_str_buffer_repeat_char(KL_StrBuffer* buff, char c, int64_t count) {
  if (buff->data == 0 || buff->capacity <= 0 || c == 0 || count <= 0) return false;
  for (int64_t i = 0; i < count; i++) {
    if (!kl_str_buffer_append_char(buff, c)) return false;
  }
  return true;
}

void kl_str_buffer_clear(KL_StrBuffer* buff) {
  if (buff->data == 0 || buff->capacity <= 0) return;
  memset(&buff->data[0], 0, buff->capacity);
  buff->length = 0;
}
