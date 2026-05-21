#ifndef KL_JSON_TOKENIZER_H
#define KL_JSON_TOKENIZER_H

#include <kl/string/string.h>
#include <kl/string/scanner.h>
#include <kl/json/token.h>
#include <stdbool.h>

bool kl_json_tokenize_next(KL_StrScanner* scanner, KL_JSONToken* out);

#endif
