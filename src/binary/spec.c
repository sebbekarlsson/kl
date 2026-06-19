#include <kl/binary/spec.h>

size_t kl_binary_type_get_size(KL_BinaryType type) {
  switch (type) {
    case KL_BINARY_NULL: return 0; 
    case KL_BINARY_INT8: return sizeof(int8_t); 
    case KL_BINARY_UINT8: return sizeof(uint8_t);
    case KL_BINARY_INT32: return sizeof(int32_t);
    case KL_BINARY_UINT32: return sizeof(uint32_t);
    case KL_BINARY_INT64: return sizeof(int64_t);
    case KL_BINARY_UINT64: return sizeof(uint64_t);
    case KL_BINARY_F32: return sizeof(float);
    case KL_BINARY_F64: return sizeof(double);
  }
  return 0;
}

size_t kl_binary_spec_get_size(KL_BinarySpec spec) {
  size_t element_size = kl_binary_type_get_size(spec.type);
  return element_size * spec.length;
}

bool kl_binary_spec_stringify_value(const uint8_t* value, KL_BinarySpec spec, KL_StrBuffer* out) {
  if (value == 0 || spec.type == KL_BINARY_NULL || spec.length <= 0) {
    return kl_str_buffer_append(out, KL_STR_CONST_LIT("NULL"));
  }

  for (int64_t i = 0; i < spec.length; i++) {
    switch (spec.type) {
      case KL_BINARY_NULL: return kl_str_buffer_append(out, KL_STR_CONST_LIT("NULL"));
      case KL_BINARY_INT8: {
        int8_t x = ((int8_t*)value)[i];
        if (spec.hint == KL_BINARY_HINT_STRING && x == 0) return out->length > 0 && out->data != 0;
        if (!kl_str_buffer_append_fmt(out, spec.hint == KL_BINARY_HINT_STRING ? "%c" : "%d", x)) return out->length > 0 && out->data != 0;
        if (spec.hint != KL_BINARY_HINT_STRING && i < (spec.length-1)) kl_str_buffer_append(out, KL_STR_CONST_LIT(", "));
      } break;
      case KL_BINARY_UINT8: {
        uint8_t x = ((uint8_t*)value)[i];
        if (spec.hint == KL_BINARY_HINT_STRING && x == 0) return out->length > 0 && out->data != 0;
        if (!kl_str_buffer_append_fmt(out, spec.hint == KL_BINARY_HINT_STRING ? "%c" : "%d", x)) return out->length > 0 && out->data != 0;
        if (spec.hint != KL_BINARY_HINT_STRING && i < (spec.length-1)) kl_str_buffer_append(out, KL_STR_CONST_LIT(", "));
      } break;
      case KL_BINARY_INT32: {
        int32_t x = ((int32_t*)value)[i];
        if (!kl_str_buffer_append_fmt(out, "%d", x)) return out->length > 0 && out->data != 0;
        if (i < (spec.length-1)) kl_str_buffer_append(out, KL_STR_CONST_LIT(", "));
      } break;
      case KL_BINARY_UINT32: {
        uint32_t x = ((uint32_t*)value)[i];
        if (!kl_str_buffer_append_fmt(out, "%d", x)) return out->length > 0 && out->data != 0;
        if (i < (spec.length-1)) kl_str_buffer_append(out, KL_STR_CONST_LIT(", "));
      } break;
      case KL_BINARY_INT64: {
        int64_t x = ((int64_t*)value)[i];
        if (!kl_str_buffer_append_fmt(out, "%ld", x)) return out->length > 0 && out->data != 0;
        if (i < (spec.length-1)) kl_str_buffer_append(out, KL_STR_CONST_LIT(", "));
      } break;
      case KL_BINARY_UINT64: {
        uint64_t x = ((uint64_t*)value)[i];
        if (!kl_str_buffer_append_fmt(out, "%ld", x)) return out->length > 0 && out->data != 0;
        if (i < (spec.length-1)) kl_str_buffer_append(out, KL_STR_CONST_LIT(", "));
      } break;
      case KL_BINARY_F32: {
        float x = ((float*)value)[i];
        if (!kl_str_buffer_append_fmt(out, "%g", x)) return out->length > 0 && out->data != 0;
        if (i < (spec.length-1)) kl_str_buffer_append(out, KL_STR_CONST_LIT(", "));
      } break;
      case KL_BINARY_F64: {
        double x = ((double*)value)[i];
        if (!kl_str_buffer_append_fmt(out, "%g", x)) return out->length > 0 && out->data != 0;
        if (i < (spec.length-1)) kl_str_buffer_append(out, KL_STR_CONST_LIT(", "));
      } break;
    }
  }


  return out->length > 0 && out->data != 0;
}
