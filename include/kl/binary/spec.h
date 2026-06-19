#ifndef KL_BINARY_SPEC_H
#define KL_BINARY_SPEC_H

#include <stdint.h>
#include <stddef.h>
#include <kl/string/string.h>

typedef enum {
  KL_BINARY_NULL,
  KL_BINARY_INT8,
  KL_BINARY_UINT8,
  KL_BINARY_INT32,
  KL_BINARY_UINT32,
  KL_BINARY_INT64,
  KL_BINARY_UINT64,
  KL_BINARY_F32,
  KL_BINARY_F64
} KL_BinaryType;

typedef enum {
  KL_BINARY_HINT_NONE,
  KL_BINARY_HINT_STRING
} KL_BinaryHint;

typedef struct {
  KL_BinaryType type;
  KL_BinaryHint hint;
  int64_t length;
} KL_BinarySpec;


size_t kl_binary_type_get_size(KL_BinaryType type);

size_t kl_binary_spec_get_size(KL_BinarySpec spec);

bool kl_binary_spec_stringify_value(const uint8_t* value, KL_BinarySpec spec, KL_StrBuffer* out);

#endif
