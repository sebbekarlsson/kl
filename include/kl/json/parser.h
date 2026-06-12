#ifndef KL_JSON_PARSER_H
#define KL_JSON_PARSER_H

#include <kl/json/node.h>
#include <kl/json/token.h>
#include <kl/string/string.h>
#include <kl/string/scanner.h>
#include <kl/memory/allocator.h>
#include <stdint.h>
#include <stdbool.h>


typedef struct {
  bool initialized;
  KL_Allocator _allocator;
  KL_Allocator *allocator;
  KL_JSONToken token;
  KL_StrScanner scanner;

  struct {
    int64_t approx_num_arrays;
    int64_t approx_num_objects;
    int64_t approx_num_strings;

    KL_JSONNode* root;
  } _;
} KL_JSONParser;

void kl_json_parser_init(KL_JSONParser* parser);

void kl_json_parser_init_with_allocator(KL_JSONParser* parser, KL_Allocator* allocator);

void kl_json_parser_destroy(KL_JSONParser* parser);

KL_JSONNode* kl_json_parser_parse(
  KL_JSONParser* parser,
  KL_StrConst content
);

#endif
