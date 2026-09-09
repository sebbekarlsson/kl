#ifndef KL_STR_H
#define KL_STR_H

#include <string.h>
#include <stdint.h>
#include <stdbool.h>
#include <kl/binary/blob.h>
#include <kl/memory/allocator.h>

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
#define KL_STR_CONST_NULL() ((KL_StrConst){ .value = 0, .length = 0 })

#define KL_STR_CONST_TO_BLOB(STR) ((KL_Blob){ .value = (uint8_t*)(STR).value, .size = (STR).length })

bool kl_str_const_eq(KL_StrConst a, KL_StrConst b);

bool kl_str_const_starts_with(KL_StrConst str, KL_StrConst sub);

bool kl_str_const_ends_with(KL_StrConst str, KL_StrConst sub);

bool kl_str_const_includes(KL_StrConst str, KL_StrConst sub);

int64_t kl_str_const_first_index_of(KL_StrConst str, KL_StrConst sub);

bool kl_str_const_is_integer(KL_StrConst str);

// True if the string is an integer or a decimal float (e.g. "3", "-3", "3.14").
bool kl_str_const_is_numeric(KL_StrConst str);

KL_StrConst kl_str_const_slice(KL_StrConst str, int64_t offset, int64_t length);

KL_StrConst kl_str_const_remove_prefix(KL_StrConst str, KL_StrConst sub);

KL_StrConst kl_str_const_remove_numeric_prefix(KL_StrConst str);

KL_StrConst kl_str_const_remove_numeric_suffix(KL_StrConst str);

KL_StrConst kl_str_const_remove_suffix(KL_StrConst str, KL_StrConst sub);

KL_StrConst kl_str_const_strdup(KL_StrConst str, KL_Allocator* allocator);

KL_StrConst kl_str_const_concat(KL_StrConst a, KL_StrConst b, KL_Allocator* allocator);

KL_StrConst kl_str_const_join(KL_StrConst a, KL_StrConst b, KL_StrConst between, KL_Allocator* allocator);

typedef struct {
  char* data;
  int64_t length;
  int64_t capacity;
  int64_t bump_size;

  /**
  * @brief used if allocator is set
  **/
  KL_Allocation allocation;
  /**
  * @brief allocator is optional
  **/
  KL_Allocator* allocator;
} KL_StrBuffer;

#define KL_STR_BUFFER_STACK_ALLOC(CAPACITY) ((KL_StrBuffer){ .data = (char*)alloca(CAPACITY), .length = 0, .capacity = (CAPACITY) - 1 })
#define KL_STR_BUFFER_TO_STR_CONST(BUFF) ((KL_StrConst){ .value = (BUFF).data, .length = (BUFF).length })

bool kl_str_buffer_fit(KL_StrBuffer* buff, int64_t extra);
int64_t kl_str_buffer_avail(const KL_StrBuffer* buff);
bool kl_str_buffer_append_char(KL_StrBuffer* buff, char c);
bool kl_str_buffer_append(KL_StrBuffer* buff, KL_StrConst content);

// Appends a printf-style formatted string to the buffer. Returns true on
// success.
bool kl_str_buffer_append_fmt(KL_StrBuffer* buff, const char* fmt, ...)
  __attribute__((format(printf, 2, 3)));

KL_StrConst kl_str_buffer_set(KL_StrBuffer* buff, KL_StrConst value);

bool kl_str_buffer_repeat_char(KL_StrBuffer* buff, char c, int64_t count);
KL_StrBuffer* kl_str_buffer_clear(KL_StrBuffer* buff);

// Renders `blob` into `buff` as text. When `json_escape` is true the output is
// a valid JSON string body (special/control chars are backslash-escaped),
// suitable for emitting between surrounding double quotes.
KL_StrBuffer* kl_str_buffer_stringify_blob(KL_StrBuffer* buff, KL_Blob blob, bool null_terminate, bool json_escape);

// Replaces occurrences of `search` with `repl`, using the caller-provided
// `buffer` as scratch space (so the call-site owns the allocation strategy).
// The buffer is cleared on entry; when `all` is true every occurrence is
// replaced, otherwise only the first. Returns a view of the buffer holding the
// result. The view is only valid until the next write to `buffer`.
KL_StrConst kl_str_const_replace(KL_StrConst str, KL_StrConst search, KL_StrConst repl, bool all, KL_StrBuffer* buffer);

#endif
