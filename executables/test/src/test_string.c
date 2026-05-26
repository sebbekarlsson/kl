#include <test/tests.h>
#include <kl/string/jaro_winkler.h>

void test_str_const_eq() {
  KL_TEST_EXPECT_EXPR(kl_str_const_eq(KL_STR_CONST_LIT("hello"), KL_STR_CONST_LIT("hello")));
  KL_TEST_EXPECT_EXPR(!kl_str_const_eq(KL_STR_CONST_LIT("hello"), KL_STR_CONST_LIT("world")));
  KL_TEST_EXPECT_EXPR(!kl_str_const_eq(KL_STR_CONST_LIT("hello"), KL_STR_CONST_LIT("hello!")));
  // empty-vs-empty: equal regardless of whether .value is a literal or null
  KL_TEST_EXPECT_EXPR(kl_str_const_eq(KL_STR_CONST_LIT(""), KL_STR_CONST_LIT("")));
  KL_TEST_EXPECT_EXPR(kl_str_const_eq(((KL_StrConst){0}), ((KL_StrConst){0})));
  KL_TEST_EXPECT_EXPR(kl_str_const_eq(KL_STR_CONST_LIT(""), ((KL_StrConst){0})));
  // non-empty vs empty: not equal
  KL_TEST_EXPECT_EXPR(!kl_str_const_eq(KL_STR_CONST_LIT("hello"), KL_STR_CONST_LIT("")));
}

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

void test_jaro_winkler_identical_returns_one() {
  KL_TEST_EXPECT_EQ_FLOAT(kl_str_const_jaro_winkler(KL_STR_CONST_LIT("hello"), KL_STR_CONST_LIT("hello"), 0.1f), 1.0f, 0.0001f);
  KL_TEST_EXPECT_EQ_FLOAT(kl_str_const_jaro_winkler(KL_STR_CONST_LIT("the quick brown fox"), KL_STR_CONST_LIT("the quick brown fox"), 0.1f), 1.0f, 0.0001f);
  KL_TEST_EXPECT_EQ_FLOAT(kl_str_const_jaro_winkler(KL_STR_CONST_LIT(""), KL_STR_CONST_LIT(""), 0.1f), 1.0f, 0.0001f);
}

void test_jaro_winkler_empty_returns_zero() {
  KL_TEST_EXPECT_EQ_FLOAT(kl_str_const_jaro_winkler(KL_STR_CONST_LIT(""), KL_STR_CONST_LIT("hello"), 0.1f), 0.0f, 0.0001f);
  KL_TEST_EXPECT_EQ_FLOAT(kl_str_const_jaro_winkler(KL_STR_CONST_LIT("hello"), KL_STR_CONST_LIT(""), 0.1f), 0.0f, 0.0001f);
}

void test_jaro_winkler_disjoint_returns_zero() {
  KL_TEST_EXPECT_EQ_FLOAT(kl_str_const_jaro_winkler(KL_STR_CONST_LIT("CAT"), KL_STR_CONST_LIT("XYZ"), 0.1f), 0.0f, 0.0001f);
}

void test_jaro_winkler_martha_marhta() {
  // Classic Winkler (1990) example: jaro ≈ 0.9444, prefix=3, JW ≈ 0.9611
  float jw = kl_str_const_jaro_winkler(KL_STR_CONST_LIT("MARTHA"), KL_STR_CONST_LIT("MARHTA"), 0.1f);
  KL_TEST_EXPECT_EQ_FLOAT(jw, 0.9611f, 0.001f);
}

void test_jaro_winkler_dixon_dicksonx() {
  // Classic Winkler (1990) example: jaro ≈ 0.7667, prefix=2, JW ≈ 0.8133
  float jw = kl_str_const_jaro_winkler(KL_STR_CONST_LIT("DIXON"), KL_STR_CONST_LIT("DICKSONX"), 0.1f);
  KL_TEST_EXPECT_EQ_FLOAT(jw, 0.8133f, 0.001f);
}

void test_jaro_winkler_is_symmetric() {
  KL_StrConst a = KL_STR_CONST_LIT("MARTHA");
  KL_StrConst b = KL_STR_CONST_LIT("MARHTA");
  KL_TEST_EXPECT_EQ_FLOAT(kl_str_const_jaro_winkler(a, b, 0.1f), kl_str_const_jaro_winkler(b, a, 0.1f), 0.0001f);
}

void test_jaro_winkler_prefix_capped_at_four() {
  // Both strings share 6 leading chars ("PREFIX"). The cap means only 4 count
  // toward the prefix boost. Hand-computed: jaro = 2.5/3 ≈ 0.8333, JW with
  // prefix=4 is 0.9. Without the cap (prefix=6) it would be ≈ 0.9333.
  float jw = kl_str_const_jaro_winkler(KL_STR_CONST_LIT("PREFIXED"), KL_STR_CONST_LIT("PREFIXAB"), 0.1f);
  KL_TEST_EXPECT_EQ_FLOAT(jw, 0.9f, 0.001f);
}

void test_jaro_winkler_stays_bounded_for_long_shared_prefix() {
  // 14-char shared prefix would push uncapped JW above 1.0 (~1.018). With the
  // cap, the result must stay ≤ 1.0.
  float jw = kl_str_const_jaro_winkler(KL_STR_CONST_LIT("ABCDEFGHIJKLMNO"), KL_STR_CONST_LIT("ABCDEFGHIJKLMNX"), 0.1f);
  KL_TEST_EXPECT_EXPR(jw <= 1.0f);
  KL_TEST_EXPECT_EXPR(jw >= 0.0f);
}

void test_jaro_winkler_single_char() {
  KL_TEST_EXPECT_EQ_FLOAT(kl_str_const_jaro_winkler(KL_STR_CONST_LIT("A"), KL_STR_CONST_LIT("A"), 0.1f), 1.0f, 0.0001f);
  KL_TEST_EXPECT_EQ_FLOAT(kl_str_const_jaro_winkler(KL_STR_CONST_LIT("A"), KL_STR_CONST_LIT("B"), 0.1f), 0.0f, 0.0001f);
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
