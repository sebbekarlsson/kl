#include <test/tests.h>

void test_json_tokenizer(KL_TestFunctionContext* ctx) {
  KL_TEST_IT(ctx, "produces the correct tokens for numbers");
  {
    KL_StrScanner scanner = {0};
    kl_str_scanner_init(&scanner, KL_STR_CONST_LIT("1 0.5"));

    KL_JSONToken tok = {0};
    char tokbuf[512] = {0};
    tok.buffer.data = tokbuf;
    tok.buffer.capacity = 512-1;

    bool ok = false;
    
    ok = kl_json_tokenize_next(&scanner, &tok);
    KL_TEST_EXPECT_EXPR(ok == true);
    KL_TEST_EXPECT_EXPR(tok.type == KL_JSON_TOKEN_INT);

    ok = kl_json_tokenize_next(&scanner, &tok);
    KL_TEST_EXPECT_EXPR(ok == true);
    KL_TEST_EXPECT_EXPR(tok.type == KL_JSON_TOKEN_FLOAT);
  }
  KL_TEST_IT_END(ctx);

  
  KL_TEST_IT(ctx, "produces the correct tokens for a simple object");
  {
    KL_StrScanner scanner = {0};
    kl_str_scanner_init(&scanner, KL_STR_CONST_LIT("{\"hello\": 1 }"));

    KL_JSONToken tok = {0};
    char tokbuf[512] = {0};
    tok.buffer.data = tokbuf;
    tok.buffer.capacity = 512-1;

    bool ok = false;
    
    ok = kl_json_tokenize_next(&scanner, &tok);
    KL_TEST_EXPECT_EXPR(ok == true);
    KL_TEST_EXPECT_EXPR(tok.type == KL_JSON_TOKEN_OBJECT_BEGIN);

    ok = kl_json_tokenize_next(&scanner, &tok);
    KL_TEST_EXPECT_EXPR(ok == true);
    KL_TEST_EXPECT_EXPR(tok.type == KL_JSON_TOKEN_STRING);

    ok = kl_json_tokenize_next(&scanner, &tok);
    KL_TEST_EXPECT_EXPR(ok == true);
    KL_TEST_EXPECT_EXPR(tok.type == KL_JSON_TOKEN_DESCRIPTOR);

    ok = kl_json_tokenize_next(&scanner, &tok);
    KL_TEST_EXPECT_EXPR(ok == true);
    KL_TEST_EXPECT_EXPR(tok.type == KL_JSON_TOKEN_INT);

    ok = kl_json_tokenize_next(&scanner, &tok);
    KL_TEST_EXPECT_EXPR(ok == true);
    KL_TEST_EXPECT_EXPR(tok.type == KL_JSON_TOKEN_OBJECT_END);
    
  }
  KL_TEST_IT_END(ctx);


  KL_TEST_IT(ctx, "produces sensible tokens for a 256KB json file");
  {
    KL_StrBuffer resolved_path = KL_STR_BUFFER_STACK_ALLOC(PATH_MAX);
    KL_StrBuffer cwd_buff = KL_STR_BUFFER_STACK_ALLOC(PATH_MAX);
    kl_path_cwd(&cwd_buff);

    
    bool did_resolve = KL_PATH_RESOLVE(&resolved_path, KL_STR_CONST(__FILE__), KL_STR_CONST_LIT("../../data/json/256KB.json"));

    KL_TEST_EXPECT_EXPR(did_resolve == true);
    KL_StrScanner scanner = {0};

    KL_Str file_contents = {0};
    printf("PATH: %s\n", resolved_path.data);
    bool file_ok = kl_fs_read_text_file((KL_StrConst){ .value = resolved_path.data, .length = resolved_path.length }, &file_contents);
    KL_TEST_EXPECT_EXPR(file_ok == true);
    assert(file_contents.value != 0);
    assert(file_contents.length > 0);
    
    kl_str_scanner_init(&scanner, (KL_StrConst){ .value = file_contents.value, .length = file_contents.length });

    // re-use this buffer
    kl_str_buffer_clear(&resolved_path);
    KL_JSONToken tok = {0};
    tok.buffer = resolved_path;

    int64_t count = 0;
    while (kl_json_tokenize_next(&scanner, &tok)) {
      count++;
    }

    KL_TEST_EXPECT_EQ_INT(count, 17425);

    if (tok.type == KL_JSON_TOKEN_TOKENIZER_ERROR) {
      fprintf(stderr, "%s\n", tok.buffer.data);
    }

    KL_TEST_EXPECT_EXPR(tok.type != KL_JSON_TOKEN_TOKENIZER_ERROR);

    free(file_contents.value);
  }
  KL_TEST_IT_END(ctx);
  //exit(1);
}

void test_json_parser(KL_TestFunctionContext* ctx) {
  KL_TEST_IT(ctx, "parses a 256KB json file without errors");
  {
    KL_StrBuffer resolved_path = KL_STR_BUFFER_STACK_ALLOC(PATH_MAX);
    KL_StrBuffer cwd_buff = KL_STR_BUFFER_STACK_ALLOC(PATH_MAX);
    kl_path_cwd(&cwd_buff);

    
    bool did_resolve = KL_PATH_RESOLVE(&resolved_path, KL_STR_CONST(__FILE__), KL_STR_CONST_LIT("../../data/json/256KB.json"));

    KL_TEST_EXPECT_EXPR(did_resolve == true);

    KL_Str file_contents = {0};
    bool file_ok = kl_fs_read_text_file((KL_StrConst){ .value = resolved_path.data, .length = resolved_path.length }, &file_contents);
    KL_TEST_EXPECT_EXPR(file_ok == true);
    assert(file_contents.value != 0);
    assert(file_contents.length > 0);

    KL_JSONParser parser = {0};
    kl_json_parser_init(&parser);
    KL_JSONNode* node = kl_json_parser_parse(&parser, (KL_StrConst){ .value = file_contents.value, .length = file_contents.length });
    //kl_str_scanner_init(&scanner, (KL_StrConst){ .value = file_contents.value, .length = file_contents.length });


    KL_TEST_EXPECT_EXPR(node != 0);

    if (node->type == KL_JSON_NODE_PARSER_ERROR) {
      printf("--> %c\n", parser.scanner.c);
    }
    
    KL_TEST_EXPECT_EXPR(node->type != KL_JSON_NODE_PARSER_ERROR);
    KL_TEST_EXPECT_EXPR(node->type == KL_JSON_NODE_ARRAY);

    #if 0
    char* stringified = (char*)calloc(file_contents.length * 2, sizeof(char));
    assert(stringified != 0);
    KL_StrBuffer b = {0};
    b.data = stringified;
    b.capacity = (file_contents.length*2)-1;
    if (!kl_json_node_stringify(node, &b)) {
      fprintf(stderr, "ERROR: Failed to stringify.\n");
      exit(1);
    }
    printf("%s\n", b.data);
    free(stringified);
    exit(0);
    #endif

    free(file_contents.value);
    kl_json_parser_destroy(&parser);
  }
  KL_TEST_IT_END(ctx);
}

void test_json_node_utils(KL_TestFunctionContext* ctx) {
  
  KL_TEST_IT(ctx, "selects an integer from an object");
  {
    KL_JSONParser parser = {0};
    kl_json_parser_init(&parser);
    KL_JSONNode* node = kl_json_parser_parse(&parser, KL_STR_CONST_LIT("{ \"foo\": 3 }"));
    KL_JSONNode* selected = kl_json_node_select(node, KL_STR_CONST_LIT("foo"));
    KL_TEST_EXPECT_EXPR(selected != 0);
    KL_TEST_EXPECT_EXPR(selected->type == KL_JSON_NODE_INT);
    kl_json_parser_destroy(&parser);
  }
  KL_TEST_IT_END(ctx);

  KL_TEST_IT(ctx, "selects a floating point number from an object");
  {
    KL_JSONParser parser = {0};
    kl_json_parser_init(&parser);
    KL_JSONNode* node = kl_json_parser_parse(&parser, KL_STR_CONST_LIT("{ \"foo\": 3.145 }"));
    KL_JSONNode* selected = kl_json_node_select(node, KL_STR_CONST_LIT("foo"));
    KL_TEST_EXPECT_EXPR(selected != 0);
    KL_TEST_EXPECT_EXPR(selected->type == KL_JSON_NODE_FLOAT);
    kl_json_parser_destroy(&parser);
  }
  KL_TEST_IT_END(ctx);

  KL_TEST_IT(ctx, "selects an integer from an array");
  {
    KL_JSONParser parser = {0};
    kl_json_parser_init(&parser);
    KL_JSONNode* node = kl_json_parser_parse(&parser, KL_STR_CONST_LIT("[5, 7, 22, 34]"));
    KL_JSONNode* selected = kl_json_node_select(node, KL_STR_CONST_LIT("1"));
    KL_TEST_EXPECT_EXPR(selected != 0);
    KL_TEST_EXPECT_EXPR(selected->type == KL_JSON_NODE_INT);
    KL_TEST_EXPECT_EXPR(selected->as.number_int == 7);
    kl_json_parser_destroy(&parser);
  }
  KL_TEST_IT_END(ctx);

  KL_TEST_IT(ctx, "returns the root node for an empty path");
  {
    KL_JSONParser parser = {0};
    kl_json_parser_init(&parser);
    KL_JSONNode* node = kl_json_parser_parse(&parser, KL_STR_CONST_LIT("{ \"foo\": 3 }"));
    KL_JSONNode* selected = kl_json_node_select(node, KL_STR_CONST_LIT(""));
    KL_TEST_EXPECT_EXPR(selected == node);
    KL_TEST_EXPECT_EXPR(selected->type == KL_JSON_NODE_OBJECT);
    kl_json_parser_destroy(&parser);
  }
  KL_TEST_IT_END(ctx);

  KL_TEST_IT(ctx, "selects a nested value using dot notation");
  {
    KL_JSONParser parser = {0};
    kl_json_parser_init(&parser);
    KL_JSONNode* node = kl_json_parser_parse(&parser, KL_STR_CONST_LIT("{ \"a\": { \"b\": { \"c\": 42 } } }"));
    KL_JSONNode* selected = kl_json_node_select(node, KL_STR_CONST_LIT("a.b.c"));
    KL_TEST_EXPECT_EXPR(selected != 0);
    KL_TEST_EXPECT_EXPR(selected->type == KL_JSON_NODE_INT);
    KL_TEST_EXPECT_EXPR(selected->as.number_int == 42);
    kl_json_parser_destroy(&parser);
  }
  KL_TEST_IT_END(ctx);

  KL_TEST_IT(ctx, "selects an array element using bracket notation");
  {
    KL_JSONParser parser = {0};
    kl_json_parser_init(&parser);
    KL_JSONNode* node = kl_json_parser_parse(&parser, KL_STR_CONST_LIT("[10, 20, 30]"));
    KL_JSONNode* selected = kl_json_node_select(node, KL_STR_CONST_LIT("[2]"));
    KL_TEST_EXPECT_EXPR(selected != 0);
    KL_TEST_EXPECT_EXPR(selected->type == KL_JSON_NODE_INT);
    KL_TEST_EXPECT_EXPR(selected->as.number_int == 30);
    kl_json_parser_destroy(&parser);
  }
  KL_TEST_IT_END(ctx);

  KL_TEST_IT(ctx, "selects through mixed object and array path segments");
  {
    KL_JSONParser parser = {0};
    kl_json_parser_init(&parser);
    KL_JSONNode* node = kl_json_parser_parse(&parser, KL_STR_CONST_LIT(
      "{ \"users\": [ { \"name\": \"alice\" }, { \"name\": \"bob\" } ] }"));
    KL_JSONNode* selected = kl_json_node_select(node, KL_STR_CONST_LIT("users[1].name"));
    KL_TEST_EXPECT_EXPR(selected != 0);
    KL_TEST_EXPECT_EXPR(selected->type == KL_JSON_NODE_STRING);
    KL_TEST_EXPECT_EXPR(kl_str_const_eq(selected->as.string, KL_STR_CONST_LIT("bob")));
    kl_json_parser_destroy(&parser);
  }
  KL_TEST_IT_END(ctx);

  KL_TEST_IT(ctx, "selects an array element nested in an object using dot index");
  {
    KL_JSONParser parser = {0};
    kl_json_parser_init(&parser);
    KL_JSONNode* node = kl_json_parser_parse(&parser, KL_STR_CONST_LIT("{ \"nums\": [9, 8, 7] }"));
    KL_JSONNode* selected = kl_json_node_select(node, KL_STR_CONST_LIT("nums.1"));
    KL_TEST_EXPECT_EXPR(selected != 0);
    KL_TEST_EXPECT_EXPR(selected->type == KL_JSON_NODE_INT);
    KL_TEST_EXPECT_EXPR(selected->as.number_int == 8);
    kl_json_parser_destroy(&parser);
  }
  KL_TEST_IT_END(ctx);

  KL_TEST_IT(ctx, "selects a value with a quoted key");
  {
    KL_JSONParser parser = {0};
    kl_json_parser_init(&parser);
    KL_JSONNode* node = kl_json_parser_parse(&parser, KL_STR_CONST_LIT("{ \"hello world\": 7 }"));
    KL_JSONNode* selected = kl_json_node_select(node, KL_STR_CONST_LIT("\"hello world\""));
    KL_TEST_EXPECT_EXPR(selected != 0);
    KL_TEST_EXPECT_EXPR(selected->type == KL_JSON_NODE_INT);
    KL_TEST_EXPECT_EXPR(selected->as.number_int == 7);
    kl_json_parser_destroy(&parser);
  }
  KL_TEST_IT_END(ctx);

  KL_TEST_IT(ctx, "selects a string value");
  {
    KL_JSONParser parser = {0};
    kl_json_parser_init(&parser);
    KL_JSONNode* node = kl_json_parser_parse(&parser, KL_STR_CONST_LIT("{ \"name\": \"karlsson\" }"));
    KL_JSONNode* selected = kl_json_node_select(node, KL_STR_CONST_LIT("name"));
    KL_TEST_EXPECT_EXPR(selected != 0);
    KL_TEST_EXPECT_EXPR(selected->type == KL_JSON_NODE_STRING);
    KL_TEST_EXPECT_EXPR(kl_str_const_eq(selected->as.string, KL_STR_CONST_LIT("karlsson")));
    kl_json_parser_destroy(&parser);
  }
  KL_TEST_IT_END(ctx);

  KL_TEST_IT(ctx, "selects a boolean value");
  {
    KL_JSONParser parser = {0};
    kl_json_parser_init(&parser);
    KL_JSONNode* node = kl_json_parser_parse(&parser, KL_STR_CONST_LIT("{ \"flag\": true }"));
    KL_JSONNode* selected = kl_json_node_select(node, KL_STR_CONST_LIT("flag"));
    KL_TEST_EXPECT_EXPR(selected != 0);
    KL_TEST_EXPECT_EXPR(selected->type == KL_JSON_NODE_BOOLEAN);
    KL_TEST_EXPECT_EXPR(selected->as.boolean == true);
    kl_json_parser_destroy(&parser);
  }
  KL_TEST_IT_END(ctx);

  KL_TEST_IT(ctx, "selects a null value");
  {
    KL_JSONParser parser = {0};
    kl_json_parser_init(&parser);
    KL_JSONNode* node = kl_json_parser_parse(&parser, KL_STR_CONST_LIT("{ \"val\": null }"));
    KL_JSONNode* selected = kl_json_node_select(node, KL_STR_CONST_LIT("val"));
    KL_TEST_EXPECT_EXPR(selected != 0);
    KL_TEST_EXPECT_EXPR(selected->type == KL_JSON_NODE_NULL);
    kl_json_parser_destroy(&parser);
  }
  KL_TEST_IT_END(ctx);

  KL_TEST_IT(ctx, "returns null for a missing object key");
  {
    KL_JSONParser parser = {0};
    kl_json_parser_init(&parser);
    KL_JSONNode* node = kl_json_parser_parse(&parser, KL_STR_CONST_LIT("{ \"foo\": 3 }"));
    KL_JSONNode* selected = kl_json_node_select(node, KL_STR_CONST_LIT("bar"));
    KL_TEST_EXPECT_EXPR(selected == 0);
    kl_json_parser_destroy(&parser);
  }
  KL_TEST_IT_END(ctx);

  KL_TEST_IT(ctx, "returns null for an out-of-bounds array index");
  {
    KL_JSONParser parser = {0};
    kl_json_parser_init(&parser);
    KL_JSONNode* node = kl_json_parser_parse(&parser, KL_STR_CONST_LIT("[1, 2, 3]"));
    KL_JSONNode* selected = kl_json_node_select(node, KL_STR_CONST_LIT("5"));
    KL_TEST_EXPECT_EXPR(selected == 0);
    kl_json_parser_destroy(&parser);
  }
  KL_TEST_IT_END(ctx);

  KL_TEST_IT(ctx, "returns null when traversing into a scalar value");
  {
    KL_JSONParser parser = {0};
    kl_json_parser_init(&parser);
    KL_JSONNode* node = kl_json_parser_parse(&parser, KL_STR_CONST_LIT("{ \"foo\": 3 }"));
    KL_JSONNode* selected = kl_json_node_select(node, KL_STR_CONST_LIT("foo.bar"));
    KL_TEST_EXPECT_EXPR(selected == 0);
    kl_json_parser_destroy(&parser);
  }
  KL_TEST_IT_END(ctx);

  KL_TEST_IT(ctx, "returns null when selecting a key from a non-traversable root");
  {
    KL_JSONParser parser = {0};
    kl_json_parser_init(&parser);
    KL_JSONNode* node = kl_json_parser_parse(&parser, KL_STR_CONST_LIT("42"));
    KL_JSONNode* selected = kl_json_node_select(node, KL_STR_CONST_LIT("foo"));
    KL_TEST_EXPECT_EXPR(selected == 0);
    kl_json_parser_destroy(&parser);
  }
  KL_TEST_IT_END(ctx);

  KL_TEST_IT(ctx, "tolerates surrounding and inner whitespace in a path");
  {
    KL_JSONParser parser = {0};
    kl_json_parser_init(&parser);
    KL_JSONNode* node = kl_json_parser_parse(&parser, KL_STR_CONST_LIT("{ \"a\": { \"b\": 5 } }"));
    KL_JSONNode* selected = kl_json_node_select(node, KL_STR_CONST_LIT("  a . b  "));
    KL_TEST_EXPECT_EXPR(selected != 0);
    KL_TEST_EXPECT_EXPR(selected->type == KL_JSON_NODE_INT);
    KL_TEST_EXPECT_EXPR(selected->as.number_int == 5);
    kl_json_parser_destroy(&parser);
  }
  KL_TEST_IT_END(ctx);

  KL_TEST_IT(ctx, "returns null for an unexpected token in a path");
  {
    KL_JSONParser parser = {0};
    kl_json_parser_init(&parser);
    KL_JSONNode* node = kl_json_parser_parse(&parser, KL_STR_CONST_LIT("{ \"a\": { \"b\": 5 } }"));
    KL_JSONNode* selected = kl_json_node_select(node, KL_STR_CONST_LIT("a@b"));
    KL_TEST_EXPECT_EXPR(selected == 0);
    kl_json_parser_destroy(&parser);
  }
  KL_TEST_IT_END(ctx);
}
