#ifndef KL_STRING_H
#define KL_STRING_H

#include <string.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
  const char* value;
  int64_t length;
} KL_StrConst;

typedef struct {
  char* value;
  int64_t length;
} KL_Str;

#define KL_STR(VALUE) ((KL_Str){ .value = VALUE, .length = strlen(VALUE) })
#define KL_STR_CONST(VALUE) ((KL_StrConst){ .value = VALUE, .length = strlen(VALUE) })
#define KL_STR_CONST_LIT(VALUE) ((KL_StrConst){ .value = VALUE, .length = sizeof(VALUE) - 1 })

bool kl_str_const_eq(KL_StrConst a, KL_StrConst b);

bool kl_str_const_starts_with(KL_StrConst str, KL_StrConst sub);

bool kl_str_const_ends_with(KL_StrConst str, KL_StrConst sub);

KL_StrConst kl_str_const_remove_prefix(KL_StrConst str, KL_StrConst sub);

KL_StrConst kl_str_const_remove_suffix(KL_StrConst str, KL_StrConst sub);

#endif
