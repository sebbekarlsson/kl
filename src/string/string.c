#include <kl/string/string.h>
#include <kl/macros.h>

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
