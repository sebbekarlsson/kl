#include <test/tests.h>

void test_str_const_slice() {
  KL_TEST_EXPECT_EXPR(kl_str_const_eq(kl_str_const_slice(KL_STR_CONST_LIT("hello world"), 0, 5), KL_STR_CONST_LIT("hello")));
  KL_TEST_EXPECT_EXPR(kl_str_const_eq(kl_str_const_slice(KL_STR_CONST_LIT("hello world"), 6, 5), KL_STR_CONST_LIT("world")));
}

void test_str_const_includes() {
  // positive cases
  KL_TEST_EXPECT_EXPR(kl_str_const_includes(KL_STR_CONST_LIT("the quick brown fox jumps over the lazy dog"), KL_STR_CONST_LIT("the")));
  KL_TEST_EXPECT_EXPR(kl_str_const_includes(KL_STR_CONST_LIT("the quick brown fox jumps over the lazy dog"), KL_STR_CONST_LIT("quick")));
  KL_TEST_EXPECT_EXPR(kl_str_const_includes(KL_STR_CONST_LIT("the quick brown fox jumps over the lazy dog"), KL_STR_CONST_LIT("brown")));
  KL_TEST_EXPECT_EXPR(kl_str_const_includes(KL_STR_CONST_LIT("the quick brown fox jumps over the lazy dog"), KL_STR_CONST_LIT("fox")));
  KL_TEST_EXPECT_EXPR(kl_str_const_includes(KL_STR_CONST_LIT("the quick brown fox jumps over the lazy dog"), KL_STR_CONST_LIT("jumps")));
  KL_TEST_EXPECT_EXPR(kl_str_const_includes(KL_STR_CONST_LIT("the quick brown fox jumps over the lazy dog"), KL_STR_CONST_LIT("over")));
  KL_TEST_EXPECT_EXPR(kl_str_const_includes(KL_STR_CONST_LIT("the quick brown fox jumps over the lazy dog"), KL_STR_CONST_LIT("the")));
  KL_TEST_EXPECT_EXPR(kl_str_const_includes(KL_STR_CONST_LIT("the quick brown fox jumps over the lazy dog"), KL_STR_CONST_LIT("lazy")));
  KL_TEST_EXPECT_EXPR(kl_str_const_includes(KL_STR_CONST_LIT("the quick brown fox jumps over the lazy dog"), KL_STR_CONST_LIT("dog")));

  // negative cases
  KL_TEST_EXPECT_EXPR(!kl_str_const_includes(KL_STR_CONST_LIT("the quick brown fox jumps over the lazy dog"), KL_STR_CONST_LIT("foo")));
  KL_TEST_EXPECT_EXPR(!kl_str_const_includes(KL_STR_CONST_LIT("the quick brown fox jumps over the lazy dog"), KL_STR_CONST_LIT("bar")));
  KL_TEST_EXPECT_EXPR(!kl_str_const_includes(KL_STR_CONST_LIT("the quick brown fox jumps over the lazy dog"), KL_STR_CONST_LIT("baz")));
  KL_TEST_EXPECT_EXPR(!kl_str_const_includes(KL_STR_CONST_LIT("the quick brown fox jumps over the lazy dog"), KL_STR_CONST_LIT("foobar")));
  KL_TEST_EXPECT_EXPR(!kl_str_const_includes(KL_STR_CONST_LIT("the quick brown fox jumps over the lazy dog"), KL_STR_CONST_LIT("bazbar")));
  KL_TEST_EXPECT_EXPR(!kl_str_const_includes(KL_STR_CONST_LIT("the quick brown fox jumps over the lazy dog"), KL_STR_CONST_LIT("foobaz")));
  KL_TEST_EXPECT_EXPR(!kl_str_const_includes(KL_STR_CONST_LIT("the quick brown fox jumps over the lazy dog"), KL_STR_CONST_LIT("bazfoo")));
  KL_TEST_EXPECT_EXPR(!kl_str_const_includes(KL_STR_CONST_LIT("the quick brown fox jumps over the lazy dog"), KL_STR_CONST_LIT("john doe")));
  KL_TEST_EXPECT_EXPR(!kl_str_const_includes(KL_STR_CONST_LIT("the quick brown fox jumps over the lazy dog"), KL_STR_CONST_LIT("abc123")));
}

void test_str_scanner(KL_TestFunctionContext* ctx) {

  KL_TEST_IT(ctx, "advances correctly");
  {
    KL_StrScanner scanner = {0};
    kl_str_scanner_init(&scanner, KL_STR_CONST_LIT("abc123"));
    KL_TEST_EXPECT_EQ_CHAR(scanner.c, 'a');
    kl_str_scanner_advance(&scanner, 1);
    KL_TEST_EXPECT_EQ_CHAR(scanner.c, 'b');
    kl_str_scanner_advance(&scanner, 1);
    KL_TEST_EXPECT_EQ_CHAR(scanner.c, 'c');
    kl_str_scanner_advance(&scanner, 1);
    KL_TEST_EXPECT_EQ_CHAR(scanner.c, '1');
    kl_str_scanner_advance(&scanner, 1);
    KL_TEST_EXPECT_EQ_CHAR(scanner.c, '2');
    KL_TEST_EXPECT_EXPR(kl_str_scanner_is_eof(&scanner) == false);
    KL_TEST_EXPECT_EXPR(kl_str_scanner_has_more(&scanner) == true);
    kl_str_scanner_advance(&scanner, 1);
    KL_TEST_EXPECT_EXPR(kl_str_scanner_has_more(&scanner) == true);
    KL_TEST_EXPECT_EQ_CHAR(scanner.c, '3');
    KL_TEST_EXPECT_EXPR(kl_str_scanner_is_eof(&scanner) == true);
    
  }
  KL_TEST_IT_END(ctx);
}
