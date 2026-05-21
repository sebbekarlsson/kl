#ifndef KL_STR_H
#define KL_STR_H

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

bool kl_str_const_includes(KL_StrConst str, KL_StrConst sub);

KL_StrConst kl_str_const_slice(KL_StrConst str, int64_t offset, int64_t length);

KL_StrConst kl_str_const_remove_prefix(KL_StrConst str, KL_StrConst sub);

KL_StrConst kl_str_const_remove_suffix(KL_StrConst str, KL_StrConst sub);

typedef struct {
  char* data;
  int64_t length;
  int64_t capacity;
} KL_StrBuffer;

#define KL_STR_BUFFER_STACK_ALLOC(CAPACITY) ((KL_StrBuffer){ .data = (char*)alloca(CAPACITY), .length = 0, .capacity = (CAPACITY) - 1 })

int64_t kl_str_buffer_avail(const KL_StrBuffer* buff);
bool kl_str_buffer_append_char(KL_StrBuffer* buff, char c);
bool kl_str_buffer_append(KL_StrBuffer* buff, KL_StrConst content);
bool kl_str_buffer_repeat_char(KL_StrBuffer* buff, char c, int64_t count);
void kl_str_buffer_clear(KL_StrBuffer* buff);

#endif
