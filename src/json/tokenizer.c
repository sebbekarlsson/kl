#include <kl/json/tokenizer.h>
#include <ctype.h>
#include <stddef.h>
#include <stdint.h>
#include <assert.h>

/* Parse a single hex digit; returns -1 if `c` is not [0-9a-fA-F]. */
static inline int hex_val(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

/* Read exactly 4 hex digits at s[p..p+3] into *out. Returns false on bad input. */
static inline bool read_hex4(const char* s, int64_t p, int64_t n, uint32_t* out) {
  if (p + 4 > n) return false;
  int h0 = hex_val(s[p]), h1 = hex_val(s[p + 1]), h2 = hex_val(s[p + 2]), h3 = hex_val(s[p + 3]);
  if (h0 < 0 || h1 < 0 || h2 < 0 || h3 < 0) return false;
  *out = (uint32_t)((h0 << 12) | (h1 << 8) | (h2 << 4) | h3);
  return true;
}

/* Append a Unicode code point to `buff` as UTF-8. */
static inline void append_utf8(KL_StrBuffer* buff, uint32_t cp) {
  if (cp <= 0x7F) {
    kl_str_buffer_append_char(buff, (char)cp);
  } else if (cp <= 0x7FF) {
    kl_str_buffer_append_char(buff, (char)(0xC0 | (cp >> 6)));
    kl_str_buffer_append_char(buff, (char)(0x80 | (cp & 0x3F)));
  } else if (cp <= 0xFFFF) {
    kl_str_buffer_append_char(buff, (char)(0xE0 | (cp >> 12)));
    kl_str_buffer_append_char(buff, (char)(0x80 | ((cp >> 6) & 0x3F)));
    kl_str_buffer_append_char(buff, (char)(0x80 | (cp & 0x3F)));
  } else {
    kl_str_buffer_append_char(buff, (char)(0xF0 | (cp >> 18)));
    kl_str_buffer_append_char(buff, (char)(0x80 | ((cp >> 12) & 0x3F)));
    kl_str_buffer_append_char(buff, (char)(0x80 | ((cp >> 6) & 0x3F)));
    kl_str_buffer_append_char(buff, (char)(0x80 | (cp & 0x3F)));
  }
}

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
    /* Scan the whole numeric span directly off the input buffer, then copy and
       advance once. Mirrors the original accept rules exactly but avoids the
       per-char append/advance overhead. The current char sits at index i-1. */
    const char* s = scanner->str.value;
    int64_t n = scanner->str.length;
    int64_t start = scanner->i - 1;
    int64_t p = start;

    out->type = KL_JSON_TOKEN_INT;

    // optional leading minus sign
    if (p < n && s[p] == '-') p++;

    // integer part
    while (p < n && isdigit((unsigned char)s[p])) p++;

    // optional fractional part
    if (p < n && s[p] == '.') {
      out->type = KL_JSON_TOKEN_FLOAT;
      p++;
      while (p < n && isdigit((unsigned char)s[p])) p++;
    }

    // optional exponent part
    if (p < n && (s[p] == 'e' || s[p] == 'E')) {
      p++;
      if (p < n && (s[p] == '+' || s[p] == '-')) p++;
      while (p < n && isdigit((unsigned char)s[p])) p++;
    }

    if (p > start) {
      kl_str_buffer_append(&out->buffer, (KL_StrConst){ .value = &s[start], .length = p - start });
      kl_str_scanner_advance(scanner, p - start);
    }

    return true;
  }

  switch (scanner->c) {
    /* Only probe a keyword when the first char can start one, instead of
       slice+memcmp-ing "true"/"false"/"null" at every token position. */
    case 't': {
      KL_StrConst tok_true = KL_STR_CONST_LIT("true");
      if (kl_str_scanner_has(scanner, tok_true)) return yield_token(scanner, out, KL_JSON_TOKEN_TRUE, tok_true);
      break;
    }
    case 'f': {
      KL_StrConst tok_false = KL_STR_CONST_LIT("false");
      if (kl_str_scanner_has(scanner, tok_false)) return yield_token(scanner, out, KL_JSON_TOKEN_FALSE, tok_false);
      break;
    }
    case 'n': {
      KL_StrConst tok_null = KL_STR_CONST_LIT("null");
      if (kl_str_scanner_has(scanner, tok_null)) return yield_token(scanner, out, KL_JSON_TOKEN_NULL, tok_null);
      break;
    }
    case '"': {
      const char* s = scanner->str.value;
      int64_t n = scanner->str.length;
      int64_t open = scanner->i - 1;   // index of the opening quote
      int64_t p = open + 1;            // first char of the string body

      for (;;) {
        // bulk-copy a run of ordinary (non-quote, non-backslash) characters
        int64_t run = p;
        while (p < n && s[p] != '"' && s[p] != '\\') p++;
        if (p > run) {
          kl_str_buffer_append(&out->buffer, (KL_StrConst){ .value = &s[run], .length = p - run });
        }

        if (p >= n) break;                // unterminated string
        if (s[p] == '"') { p++; break; }  // consume the closing quote

        // s[p] == '\\' : decode the escape sequence
        p++;                              // skip the backslash
        if (p >= n) break;                // dangling backslash at EOF
        char e = s[p];
        switch (e) {
          case '"':  kl_str_buffer_append_char(&out->buffer, '"');  p++; break;
          case '\\': kl_str_buffer_append_char(&out->buffer, '\\'); p++; break;
          case '/':  kl_str_buffer_append_char(&out->buffer, '/');  p++; break;
          case 'b':  kl_str_buffer_append_char(&out->buffer, '\b'); p++; break;
          case 'f':  kl_str_buffer_append_char(&out->buffer, '\f'); p++; break;
          case 'n':  kl_str_buffer_append_char(&out->buffer, '\n'); p++; break;
          case 'r':  kl_str_buffer_append_char(&out->buffer, '\r'); p++; break;
          case 't':  kl_str_buffer_append_char(&out->buffer, '\t'); p++; break;
          case 'u': {
            uint32_t cp = 0;
            if (!read_hex4(s, p + 1, n, &cp)) {
              // malformed \uXXXX: keep the 'u' literally and carry on
              kl_str_buffer_append_char(&out->buffer, 'u');
              p++;
              break;
            }
            p += 5;   // consume 'u' + 4 hex digits
            // combine a UTF-16 surrogate pair (\uD800-\uDBFF followed by \uDC00-\uDFFF)
            if (cp >= 0xD800 && cp <= 0xDBFF && p + 1 < n && s[p] == '\\' && s[p + 1] == 'u') {
              uint32_t lo = 0;
              if (read_hex4(s, p + 2, n, &lo) && lo >= 0xDC00 && lo <= 0xDFFF) {
                cp = 0x10000 + (((cp - 0xD800) << 10) | (lo - 0xDC00));
                p += 6;   // consume the trailing \uXXXX low surrogate
              }
            }
            append_utf8(&out->buffer, cp);
            break;
          }
          default:
            // unknown escape: keep the following char literally (lenient)
            kl_str_buffer_append_char(&out->buffer, e);
            p++;
            break;
        }
      }

      // sync the scanner to the final position: just past the closing quote, or
      // at EOF for an unterminated string. advance() is O(1), so a single call.
      kl_str_scanner_advance(scanner, p - open);

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
