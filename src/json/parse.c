#include <kl/json/parser.h>
#include <kl/json/tokenizer.h>
#include <kl/macros.h>
#include <string.h>
#include <ctype.h>
#include <stddef.h>
#include <assert.h>
#include <stdalign.h>
#include <stdio.h>

void kl_json_parser_init(KL_JSONParser* parser) {
  if (parser->initialized) return;

  // we only zero out the struct
  // and intentionally don't initialize the allocator here because
  // we can later try to infer the best capactiy from the content
  memset(parser, 0, sizeof(KL_JSONParser));
  parser->allocator = &parser->_allocator;
  parser->initialized = true;
}

void kl_json_parser_init_with_allocator(KL_JSONParser* parser, KL_Allocator* allocator) {
  if (parser->initialized) return;
  kl_json_parser_init(parser);
  parser->allocator = allocator;
}

void kl_json_parser_destroy(KL_JSONParser* parser) {
  assert(parser->initialized);
  if (parser->_allocator.initialized) {
    kl_allocator_destroy(&parser->_allocator);
  }

  if (parser->token.buffer.data != 0) {
    free(parser->token.buffer.data);
  }
  parser->token.buffer.data = 0;
  parser->token.buffer.length = 0;
  parser->token.buffer.capacity = 0;
  memset(parser, 0, sizeof(KL_JSONParser));
}


static KL_JSONNode* create_node(KL_JSONParser* parser, KL_JSONNode cfg) {
  assert(parser->allocator != 0);
  
  KL_JSONNode* node = (KL_JSONNode*)kl_allocator_malloc_aligned(parser->allocator, sizeof(KL_JSONNode), alignof(KL_JSONNode));
  assert(node != 0);
  memcpy(node, &cfg, sizeof(KL_JSONNode));
  return node;
}

static bool eat(KL_JSONParser* parser, KL_JSONTokenType toktype) {
  if (parser->token.type != toktype) {
    fprintf(stderr, "Expected token `%d` but got `%d`\n", toktype, parser->token.type);
    return false;
  }

  return kl_json_tokenize_next(&parser->scanner, &parser->token);
}

static KL_JSONNode* parse(KL_JSONParser* parser);

static KL_JSONNode* parse_string(KL_JSONParser* parser) {
  KL_JSONNode* node = create_node(parser, (KL_JSONNode){ .type = KL_JSON_NODE_STRING });
  if (parser->token.buffer.length > 0) {
    node->as.string.value = kl_allocator_strdup(parser->allocator, parser->token.buffer.data, parser->token.buffer.length);
    node->as.string.length = parser->token.buffer.length;
  }
  eat(parser, KL_JSON_TOKEN_STRING);
  return node;
}


static KL_JSONNode* parse_obj_member(KL_JSONParser* parser, KL_JSONNode* obj) {
  if (parser->token.type != KL_JSON_TOKEN_STRING) return create_node(parser, (KL_JSONNode){ .type = KL_JSON_NODE_PARSER_ERROR });
  char* key = (char*)kl_allocator_strdup(parser->allocator, parser->token.buffer.data, parser->token.buffer.length);
  assert(key != 0);
  KL_StrConst k = {
    .value = key,
    .length = parser->token.buffer.length
  };
  if (!eat(parser, KL_JSON_TOKEN_STRING)) return create_node(parser, (KL_JSONNode){ .type = KL_JSON_NODE_PARSER_ERROR });
  if (!eat(parser, KL_JSON_TOKEN_DESCRIPTOR)) return create_node(parser, (KL_JSONNode){ .type = KL_JSON_NODE_PARSER_ERROR });
  KL_JSONNode* value = parse(parser);
  assert(value != 0);
  if (value->type == KL_JSON_NODE_PARSER_ERROR) return value;
  kl_json_node_ptr_hashmap_set(&obj->as.object, (KL_Blob){ .value = (uint8_t*)k.value, .size = k.length }, value);
  return value;
}

static KL_JSONNode* parse(KL_JSONParser* parser) {
  switch (parser->token.type) {
    case KL_JSON_TOKEN_STRING: return parse_string(parser); 
    case KL_JSON_TOKEN_INT: {
      KL_JSONNode* node = create_node(parser, (KL_JSONNode){ .type = KL_JSON_NODE_INT });
      if (parser->token.buffer.length > 0) {
        node->as.number_int = (int64_t)atoll(parser->token.buffer.data);  
      }
      eat(parser, KL_JSON_TOKEN_INT);
      return node;
    };
    case KL_JSON_TOKEN_FLOAT: {
      KL_JSONNode* node = create_node(parser, (KL_JSONNode){ .type = KL_JSON_NODE_FLOAT });
      if (parser->token.buffer.length > 0) {
        node->as.number_float = atof(parser->token.buffer.data);  
      }
      eat(parser, KL_JSON_TOKEN_FLOAT);
      return node;
    };
    case KL_JSON_TOKEN_FALSE:
    case KL_JSON_TOKEN_TRUE: {
      KL_JSONNode* node = create_node(parser, (KL_JSONNode){ .type = KL_JSON_NODE_BOOLEAN });
      node->as.boolean = (parser->token.type == KL_JSON_TOKEN_TRUE);
      eat(parser, parser->token.type == KL_JSON_TOKEN_TRUE ? KL_JSON_TOKEN_TRUE : KL_JSON_TOKEN_FALSE);
      return node;
    }
    case KL_JSON_TOKEN_NULL: {
      KL_JSONNode* node = create_node(parser, (KL_JSONNode){ .type = KL_JSON_NODE_NULL });
      eat(parser, KL_JSON_TOKEN_NULL);
      return node;
    };
    case KL_JSON_TOKEN_ARRAY_BEGIN: {
      if (!eat(parser, KL_JSON_TOKEN_ARRAY_BEGIN)) return create_node(parser, (KL_JSONNode){ .type = KL_JSON_NODE_PARSER_ERROR });
      KL_JSONNode* arr = create_node(parser, (KL_JSONNode){ .type = KL_JSON_NODE_ARRAY });
      kl_json_node_ptr_list_init(&arr->as.array, (KL_ListConfig){
        .capacity = 4,
        .allocator = parser->allocator
      });

      if (parser->token.type == KL_JSON_TOKEN_ARRAY_END) {
        eat(parser, KL_JSON_TOKEN_ARRAY_END);
        return arr;
      }

      KL_JSONNode* item = parse(parser);
      assert(item != 0);
      if (item->type == KL_JSON_NODE_PARSER_ERROR) return item;
      kl_json_node_ptr_list_push(&arr->as.array, item);

      while (parser->token.type == KL_JSON_TOKEN_SEPARATOR) {
        if (!eat(parser, KL_JSON_TOKEN_SEPARATOR)) return create_node(parser, (KL_JSONNode){ .type = KL_JSON_NODE_PARSER_ERROR }); 
        KL_JSONNode* item = parse(parser);
        assert(item != 0);
        if (item->type == KL_JSON_NODE_PARSER_ERROR) return item;
        kl_json_node_ptr_list_push(&arr->as.array, item);
      }

      eat(parser, KL_JSON_TOKEN_ARRAY_END);

      return arr;
    };
    case KL_JSON_TOKEN_OBJECT_BEGIN: {
      if (!eat(parser, KL_JSON_TOKEN_OBJECT_BEGIN)) return create_node(parser, (KL_JSONNode){ .type = KL_JSON_NODE_PARSER_ERROR });
      KL_JSONNode* obj = create_node(parser, (KL_JSONNode){ .type = KL_JSON_NODE_OBJECT });
      kl_json_node_ptr_hashmap_init(&obj->as.object, (KL_HashmapConfig){
        .capacity = KL_MAX(73, parser->_.approx_num_strings),
        .collision_capacity = 4,
        .allocator = parser->allocator,
        .track_used_slots = true
      });
      if (parser->token.type == KL_JSON_TOKEN_OBJECT_END) {
        eat(parser, KL_JSON_TOKEN_OBJECT_END);
        return obj;
      }


      if (parser->token.type != KL_JSON_TOKEN_STRING) return create_node(parser, (KL_JSONNode){ .type = KL_JSON_NODE_PARSER_ERROR });

      parse_obj_member(parser, obj);

      while (parser->token.type == KL_JSON_TOKEN_SEPARATOR) {
        if (!eat(parser, KL_JSON_TOKEN_SEPARATOR)) return create_node(parser, (KL_JSONNode){ .type = KL_JSON_NODE_PARSER_ERROR }); 
        parse_obj_member(parser, obj);
      }
      
      eat(parser, KL_JSON_TOKEN_OBJECT_END);

      return obj;
    };
    case KL_JSON_TOKEN_TOKENIZER_ERROR: return create_node(parser, (KL_JSONNode){ .type = KL_JSON_NODE_PARSER_ERROR });
    case KL_JSON_TOKEN_NONE: return create_node(parser, (KL_JSONNode){ .type = KL_JSON_NODE_PARSER_ERROR });
    default: return create_node(parser, (KL_JSONNode){ .type = KL_JSON_NODE_PARSER_ERROR }); 
  }

  return create_node(parser, (KL_JSONNode){ .type = KL_JSON_NODE_PARSER_ERROR });
}


KL_JSONNode* kl_json_parser_parse(
  KL_JSONParser* parser,
  KL_StrConst content
) {
  assert(parser->initialized);
  assert(parser->allocator != 0);
  
  if (content.value == 0 || content.length <= 0) return 0;

  if (!kl_str_scanner_init(&parser->scanner, content)) return 0;

  int64_t approx_num_arrays = 1;
  int64_t approx_num_objects = 1;
  int64_t approx_num_strings = 1;
  for (int64_t i = 0; i < content.length; i++) {
    char c = content.value[i];
    approx_num_arrays += (int64_t)(c == '[');
    approx_num_objects += (int64_t)(c == '{');
    approx_num_objects += (int64_t)(c == '"');
  }
  parser->_.approx_num_arrays = approx_num_arrays;
  parser->_.approx_num_objects = approx_num_objects;
  parser->_.approx_num_strings = approx_num_strings;

  if (!parser->allocator->initialized) {
    kl_allocator_init(parser->allocator, (KL_AllocatorConfig){
      .capacity = (
        (
          + approx_num_arrays
          + approx_num_objects
          + approx_num_strings
        ) * sizeof(KL_JSONNode)
      ) + content.length
    });
  }

  parser->token.buffer.data = (char*)calloc(content.length + 1, sizeof(char));
  assert(parser->token.buffer.data != 0);
  parser->token.buffer.capacity = content.length;
  parser->token.buffer.length = 0;

  kl_json_tokenize_next(&parser->scanner, &parser->token);

  KL_JSONNode* root = parse(parser);

  parser->_.root = root;
  
  return root;
}
