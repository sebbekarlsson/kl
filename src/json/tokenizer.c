#include <kl/json/tokenizer.h>
#include <ctype.h>
#include <stddef.h>
#include <assert.h>

static inline bool yield_token(KL_StrScanner* scanner, KL_JSONToken* out, KL_JSONTokenType type, KL_StrConst value) {
  out->type = type;
  kl_str_buffer_append(&out->buffer, value);
  kl_str_scanner_advance(scanner, value.length);
  return true;
}

bool kl_json_tokenize_next(KL_StrScanner* scanner, KL_JSONToken* out) {
  if (!kl_str_scanner_has_more(scanner)) return false;
  if (!kl_str_scanner_skip_whitespace(scanner)) return false;

  out->type = KL_JSON_TOKEN_NONE;
  kl_str_buffer_clear(&out->buffer);

  if (isdigit(scanner->c) || scanner->c == '-') {
    out->type = KL_JSON_TOKEN_NUMBER;

    // optional leading minus sign
    if (scanner->c == '-') {
      kl_str_buffer_append_char(&out->buffer, scanner->c);
      kl_str_scanner_advance(scanner, 1);
    }

    // integer part
    while (!kl_str_scanner_is_eof(scanner) && isdigit(scanner->c)) {
      kl_str_buffer_append_char(&out->buffer, scanner->c);
      kl_str_scanner_advance(scanner, 1);
    }

    // optional fractional part
    if (!kl_str_scanner_is_eof(scanner) && scanner->c == '.') {
      kl_str_buffer_append_char(&out->buffer, scanner->c);
      kl_str_scanner_advance(scanner, 1);
      while (!kl_str_scanner_is_eof(scanner) && isdigit(scanner->c)) {
        kl_str_buffer_append_char(&out->buffer, scanner->c);
        kl_str_scanner_advance(scanner, 1);
      }
    }

    // optional exponent part
    if (!kl_str_scanner_is_eof(scanner) && (scanner->c == 'e' || scanner->c == 'E')) {
      kl_str_buffer_append_char(&out->buffer, scanner->c);
      kl_str_scanner_advance(scanner, 1);
      if (!kl_str_scanner_is_eof(scanner) && (scanner->c == '+' || scanner->c == '-')) {
        kl_str_buffer_append_char(&out->buffer, scanner->c);
        kl_str_scanner_advance(scanner, 1);
      }
      while (!kl_str_scanner_is_eof(scanner) && isdigit(scanner->c)) {
        kl_str_buffer_append_char(&out->buffer, scanner->c);
        kl_str_scanner_advance(scanner, 1);
      }
    }

    return true;
  }

  KL_StrConst tok_true = KL_STR_CONST_LIT("true");
  if (kl_str_scanner_has(scanner, tok_true)) {
    return yield_token(scanner, out, KL_JSON_TOKEN_TRUE, tok_true);
  }
  
  KL_StrConst tok_false = KL_STR_CONST_LIT("false");
  if (kl_str_scanner_has(scanner, tok_false)) {
    return yield_token(scanner, out, KL_JSON_TOKEN_FALSE, tok_false);
  }
  
  KL_StrConst tok_null = KL_STR_CONST_LIT("null");
  if (kl_str_scanner_has(scanner, tok_null)) {
    return yield_token(scanner, out, KL_JSON_TOKEN_NULL, tok_null);
  }

  switch (scanner->c) {
    case '"': {
      if (!kl_str_scanner_advance(scanner, 1)) return false;
      
      while (kl_str_scanner_has_more(scanner) && scanner->c != '"') {
        if (scanner->c == '\\') {
          kl_str_scanner_advance(scanner, 1);
          continue;
        }
        kl_str_buffer_append_char(&out->buffer, scanner->c);
        kl_str_scanner_advance(scanner, 1);
      }

      if (scanner->c == '"') {
        kl_str_scanner_advance(scanner, 1);
      }

      out->type = KL_JSON_TOKEN_STRING;
      return true;
    };
    case '{': return yield_token(scanner, out, KL_JSON_TOKEN_OBJECT_BEGIN, KL_STR_CONST_LIT("{"));
    case '}': return yield_token(scanner, out, KL_JSON_TOKEN_OBJECT_END, KL_STR_CONST_LIT("}"));
    case '[': return yield_token(scanner, out, KL_JSON_TOKEN_ARRAY_BEGIN, KL_STR_CONST_LIT("["));
    case ']': return yield_token(scanner, out, KL_JSON_TOKEN_ARRAY_END, KL_STR_CONST_LIT("]"));
    case ':': return yield_token(scanner, out, KL_JSON_TOKEN_DESCRIPTOR, KL_STR_CONST_LIT(":"));
    case ',': return yield_token(scanner, out, KL_JSON_TOKEN_SEPARATOR, KL_STR_CONST_LIT(","));
  }

  out->type = KL_JSON_TOKEN_TOKENIZER_ERROR;
  kl_str_buffer_append(&out->buffer, KL_STR_CONST_LIT("[ERROR]: Unexpected token: "));
  kl_str_buffer_append_char(&out->buffer, scanner->c);

  return false;
}
