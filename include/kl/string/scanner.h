#ifndef KL_STR_SCANNER_H
#define KL_STR_SCANNER_H

#include <kl/string/string.h>
#include <stdint.h>
#include <stdbool.h>

typedef struct {
  bool initialized;
  KL_StrConst str;
  int64_t i;
  char c;
} KL_StrScanner;


bool kl_str_scanner_init(KL_StrScanner* scanner, KL_StrConst str);

/**
* @brief returns true if it reached the end of the content
**/
bool kl_str_scanner_is_eof(KL_StrScanner* scanner);

/**
* @brief returns false if it can't advance more
**/
bool kl_str_scanner_has_more(KL_StrScanner* scanner);

/**
* @brief mutates the state
**/
bool kl_str_scanner_advance(KL_StrScanner* scanner, int64_t steps);

/**
* @brief mutates the state
**/
bool kl_str_scanner_skip_whitespace(KL_StrScanner* scanner);

/**
* @brief does not mutate the state
**/
char kl_str_scanner_peek(KL_StrScanner* scanner, int64_t offset);

/**
* @brief does not mutate the state
**/
KL_StrConst kl_str_scanner_slice(KL_StrScanner* scanner, int64_t length);

/**
* @brief does not mutate the state
**/
bool kl_str_scanner_has(KL_StrScanner* scanner, KL_StrConst token);

#endif
