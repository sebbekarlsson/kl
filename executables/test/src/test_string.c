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

void test_str_const_remove_numeric_prefix(KL_TestFunctionContext* ctx) {
  KL_TEST_EXPECT_EXPR(kl_str_const_eq(kl_str_const_remove_numeric_prefix(KL_STR_CONST_LIT("04hello.wav")), KL_STR_CONST_LIT("hello.wav")));
  KL_TEST_EXPECT_EXPR(kl_str_const_eq(kl_str_const_remove_numeric_prefix(KL_STR_CONST_LIT("0.4hello.wav")), KL_STR_CONST_LIT("hello.wav")));
  KL_TEST_EXPECT_EXPR(kl_str_const_eq(kl_str_const_remove_numeric_prefix(KL_STR_CONST_LIT("6hello.wav")), KL_STR_CONST_LIT("hello.wav")));
  KL_TEST_EXPECT_EXPR(kl_str_const_eq(kl_str_const_remove_numeric_prefix(KL_STR_CONST_LIT("3.356hello.wav")), KL_STR_CONST_LIT("hello.wav")));
}


void test_str_const_remove_numeric_suffix(KL_TestFunctionContext* ctx) {
  KL_TEST_EXPECT_EXPR(kl_str_const_eq(kl_str_const_remove_numeric_suffix(KL_STR_CONST_LIT("hello04")), KL_STR_CONST_LIT("hello")));
  KL_TEST_EXPECT_EXPR(kl_str_const_eq(kl_str_const_remove_numeric_suffix(KL_STR_CONST_LIT("hello0.4")), KL_STR_CONST_LIT("hello")));
  KL_TEST_EXPECT_EXPR(kl_str_const_eq(kl_str_const_remove_numeric_suffix(KL_STR_CONST_LIT("hello6")), KL_STR_CONST_LIT("hello")));
  KL_TEST_EXPECT_EXPR(kl_str_const_eq(kl_str_const_remove_numeric_suffix(KL_STR_CONST_LIT("hello3.356")), KL_STR_CONST_LIT("hello")));
  KL_TEST_EXPECT_EXPR(kl_str_const_eq(kl_str_const_remove_numeric_suffix(KL_STR_CONST_LIT("hello.wav")), KL_STR_CONST_LIT("hello.wav")));
  KL_TEST_EXPECT_EXPR(kl_str_const_eq(kl_str_const_remove_numeric_suffix(KL_STR_CONST_LIT("hello")), KL_STR_CONST_LIT("hello")));
}


void test_str_const_replace(KL_TestFunctionContext* ctx) {
  KL_Allocator allocator = {0};
  kl_allocator_init(&allocator, (KL_AllocatorConfig){ .capacity = 1024 });

  // The buffer is scratch space; kl_str_const_replace clears it on entry, so it
  // can be reused across every call below.
  KL_StrBuffer buffer = {0};
  buffer.allocator = &allocator;

  // --- single replacement (all = false) ---
  // match at the start
  KL_TEST_EXPECT_EQ_STR(kl_str_const_replace(KL_STR_CONST_LIT("foo bar"), KL_STR_CONST_LIT("foo"), KL_STR_CONST_LIT("baz"), false, &buffer), KL_STR_CONST_LIT("baz bar"));
  // match at the end
  KL_TEST_EXPECT_EQ_STR(kl_str_const_replace(KL_STR_CONST_LIT("bar foo"), KL_STR_CONST_LIT("foo"), KL_STR_CONST_LIT("baz"), false, &buffer), KL_STR_CONST_LIT("bar baz"));
  // match in the middle
  KL_TEST_EXPECT_EQ_STR(kl_str_const_replace(KL_STR_CONST_LIT("abcXYZdef"), KL_STR_CONST_LIT("XYZ"), KL_STR_CONST_LIT("-"), false, &buffer), KL_STR_CONST_LIT("abc-def"));
  // search spans the entire string
  KL_TEST_EXPECT_EQ_STR(kl_str_const_replace(KL_STR_CONST_LIT("foo"), KL_STR_CONST_LIT("foo"), KL_STR_CONST_LIT("bar"), false, &buffer), KL_STR_CONST_LIT("bar"));
  // replacement longer than the search
  KL_TEST_EXPECT_EQ_STR(kl_str_const_replace(KL_STR_CONST_LIT("a-b"), KL_STR_CONST_LIT("-"), KL_STR_CONST_LIT("___"), false, &buffer), KL_STR_CONST_LIT("a___b"));
  // replacement shorter than the search
  KL_TEST_EXPECT_EQ_STR(kl_str_const_replace(KL_STR_CONST_LIT("hello world"), KL_STR_CONST_LIT("world"), KL_STR_CONST_LIT("!"), false, &buffer), KL_STR_CONST_LIT("hello !"));
  // only the first occurrence is replaced
  KL_TEST_EXPECT_EQ_STR(kl_str_const_replace(KL_STR_CONST_LIT("foo foo"), KL_STR_CONST_LIT("foo"), KL_STR_CONST_LIT("baz"), false, &buffer), KL_STR_CONST_LIT("baz foo"));
  // no match leaves the string unchanged
  KL_TEST_EXPECT_EQ_STR(kl_str_const_replace(KL_STR_CONST_LIT("hello"), KL_STR_CONST_LIT("xyz"), KL_STR_CONST_LIT("abc"), false, &buffer), KL_STR_CONST_LIT("hello"));

  KL_TEST_EXPECT_EQ_STR(kl_str_const_replace(KL_STR_CONST_LIT("foobar.wav"), KL_STR_CONST_LIT(".wav"), KL_STR_CONST_LIT(""), false, &buffer), KL_STR_CONST_LIT("foobar"));

  // --- replace-all (all = true) ---
  // every occurrence is replaced
  KL_TEST_EXPECT_EQ_STR(kl_str_const_replace(KL_STR_CONST_LIT("foo foo"), KL_STR_CONST_LIT("foo"), KL_STR_CONST_LIT("baz"), true, &buffer), KL_STR_CONST_LIT("baz baz"));
  // matches at start, middle and end
  KL_TEST_EXPECT_EQ_STR(kl_str_const_replace(KL_STR_CONST_LIT("a.b.c.d"), KL_STR_CONST_LIT("."), KL_STR_CONST_LIT("/"), true, &buffer), KL_STR_CONST_LIT("a/b/c/d"));
  // adjacent matches
  KL_TEST_EXPECT_EQ_STR(kl_str_const_replace(KL_STR_CONST_LIT("aaaa"), KL_STR_CONST_LIT("a"), KL_STR_CONST_LIT("bb"), true, &buffer), KL_STR_CONST_LIT("bbbbbbbb"));
  // multi-char search with a longer replacement
  KL_TEST_EXPECT_EQ_STR(kl_str_const_replace(KL_STR_CONST_LIT("x and y and z"), KL_STR_CONST_LIT("and"), KL_STR_CONST_LIT("AND"), true, &buffer), KL_STR_CONST_LIT("x AND y AND z"));
  // replacement contains the search text: only the original occurrences are replaced (no infinite loop)
  KL_TEST_EXPECT_EQ_STR(kl_str_const_replace(KL_STR_CONST_LIT("a a"), KL_STR_CONST_LIT("a"), KL_STR_CONST_LIT("aa"), true, &buffer), KL_STR_CONST_LIT("aa aa"));
  // no match leaves the string unchanged
  KL_TEST_EXPECT_EQ_STR(kl_str_const_replace(KL_STR_CONST_LIT("hello"), KL_STR_CONST_LIT("xyz"), KL_STR_CONST_LIT("abc"), true, &buffer), KL_STR_CONST_LIT("hello"));

  // --- more replace-all stress cases ---
  // many single-char occurrences
  KL_TEST_EXPECT_EQ_STR(kl_str_const_replace(KL_STR_CONST_LIT("a,b,c,d,e"), KL_STR_CONST_LIT(","), KL_STR_CONST_LIT(";"), true, &buffer), KL_STR_CONST_LIT("a;b;c;d;e"));
  // multi-char search collapsing to a shorter replacement, back to back
  KL_TEST_EXPECT_EQ_STR(kl_str_const_replace(KL_STR_CONST_LIT("foofoofoo"), KL_STR_CONST_LIT("foo"), KL_STR_CONST_LIT("x"), true, &buffer), KL_STR_CONST_LIT("xxx"));
  // multi-char search with multi-char replacement, no separators
  KL_TEST_EXPECT_EQ_STR(kl_str_const_replace(KL_STR_CONST_LIT("ababab"), KL_STR_CONST_LIT("ab"), KL_STR_CONST_LIT("CD"), true, &buffer), KL_STR_CONST_LIT("CDCDCD"));
  // matches are consumed, so overlapping candidates do not re-match: "aaa"/"aa" -> "b" + leftover "a"
  KL_TEST_EXPECT_EQ_STR(kl_str_const_replace(KL_STR_CONST_LIT("aaa"), KL_STR_CONST_LIT("aa"), KL_STR_CONST_LIT("b"), true, &buffer), KL_STR_CONST_LIT("ba"));
  // interleaved matches and non-matching text are both preserved
  KL_TEST_EXPECT_EQ_STR(kl_str_const_replace(KL_STR_CONST_LIT("the cat sat on the mat"), KL_STR_CONST_LIT("at"), KL_STR_CONST_LIT("AT"), true, &buffer), KL_STR_CONST_LIT("the cAT sAT on the mAT"));
  // every character matches
  KL_TEST_EXPECT_EQ_STR(kl_str_const_replace(KL_STR_CONST_LIT("aaaaa"), KL_STR_CONST_LIT("a"), KL_STR_CONST_LIT("a"), true, &buffer), KL_STR_CONST_LIT("aaaaa"));
  // same input, all=false vs all=true, to contrast the two modes
  KL_TEST_EXPECT_EQ_STR(kl_str_const_replace(KL_STR_CONST_LIT("x.y.z"), KL_STR_CONST_LIT("."), KL_STR_CONST_LIT("/"), false, &buffer), KL_STR_CONST_LIT("x/y.z"));
  KL_TEST_EXPECT_EQ_STR(kl_str_const_replace(KL_STR_CONST_LIT("x.y.z"), KL_STR_CONST_LIT("."), KL_STR_CONST_LIT("/"), true, &buffer), KL_STR_CONST_LIT("x/y/z"));

  kl_allocator_destroy(&allocator);
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

void test_str_buffer_grows_geometrically(KL_TestFunctionContext* ctx) {
  KL_Allocator allocator = {0};
  kl_allocator_init(&allocator, (KL_AllocatorConfig){ .capacity = 1024 });

  // A small fixed step must not make a long buffer grow linearly: in an arena
  // every old copy stays allocated, so that's quadratic memory.
  KL_StrBuffer buffer = { .allocator = &allocator, .bump_size = 16 };
  int64_t grows = 0;
  int64_t capacity = buffer.capacity;
  for (int64_t i = 0; i < 1000000; i++) {
    KL_TEST_EXPECT_EXPR(kl_str_buffer_append_char(&buffer, 'a' + (char)(i % 26)));
    if (buffer.capacity != capacity) {
      grows++;
      capacity = buffer.capacity;
    }
  }
  KL_TEST_EXPECT_EQ_INT(buffer.length, 1000000);
  KL_TEST_EXPECT_EQ_CHAR(buffer.data[buffer.length], '\0');
  KL_TEST_EXPECT_EQ_CHAR(buffer.data[999999], 'a' + (char)(999999 % 26));
  KL_TEST_ASSERT(grows < 40, "capacity doubles");
  KL_TEST_ASSERT(buffer.capacity < 4 * 1000000, "capacity stays within a constant factor");

  kl_allocator_destroy(&allocator);
}

void test_str_buffer_append_fmt(KL_TestFunctionContext* ctx) {
  KL_Allocator allocator = {0};
  kl_allocator_init(&allocator, (KL_AllocatorConfig){ .capacity = 1024 });

  KL_StrBuffer buffer = {0};
  buffer.allocator = &allocator;

  KL_TEST_IT(ctx, "grows a zero-length buffer from scratch");
  {
    // No backing storage yet — only an allocator. append_fmt must allocate.
    KL_TEST_EXPECT_EXPR(buffer.data == 0);
    KL_TEST_EXPECT_EQ_INT(buffer.capacity, 0);
    KL_TEST_EXPECT_EXPR(kl_str_buffer_append_fmt(&buffer, "%d-%s", 42, "hi"));
    KL_TEST_EXPECT_EXPR(buffer.data != 0);
    KL_TEST_EXPECT_EQ_STR(((KL_StrConst){ .value = buffer.data, .length = buffer.length }), KL_STR_CONST_LIT("42-hi"));
  }
  KL_TEST_IT_END(ctx);

  KL_TEST_IT(ctx, "formats and appends");
  {
    kl_str_buffer_clear(&buffer);
    KL_TEST_EXPECT_EXPR(kl_str_buffer_append_fmt(&buffer, "%d-%s", 42, "hi"));
    KL_TEST_EXPECT_EQ_STR(((KL_StrConst){ .value = buffer.data, .length = buffer.length }), KL_STR_CONST_LIT("42-hi"));
  }
  KL_TEST_IT_END(ctx);

  KL_TEST_IT(ctx, "appends onto existing content");
  {
    KL_TEST_EXPECT_EXPR(kl_str_buffer_append_fmt(&buffer, " %.2f", 1.5));
    KL_TEST_EXPECT_EQ_STR(((KL_StrConst){ .value = buffer.data, .length = buffer.length }), KL_STR_CONST_LIT("42-hi 1.50"));
  }
  KL_TEST_IT_END(ctx);

  KL_TEST_IT(ctx, "grows past the initial capacity");
  {
    kl_str_buffer_clear(&buffer);
    // %500s right-pads to 500 chars, forcing growth beyond the bump size.
    KL_TEST_EXPECT_EXPR(kl_str_buffer_append_fmt(&buffer, "%500s", ""));
    KL_TEST_EXPECT_EQ_INT(buffer.length, 500);
    KL_TEST_EXPECT_EQ_CHAR(buffer.data[buffer.length], '\0');
  }
  KL_TEST_IT_END(ctx);

  KL_TEST_IT(ctx, "empty result appends nothing");
  {
    kl_str_buffer_clear(&buffer);
    KL_TEST_EXPECT_EXPR(kl_str_buffer_append_fmt(&buffer, "%s", ""));
    KL_TEST_EXPECT_EQ_INT(buffer.length, 0);
  }
  KL_TEST_IT_END(ctx);

  kl_allocator_destroy(&allocator);
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

void test_str_const_is_numeric_accepts_exponents(KL_TestFunctionContext* ctx) {
  KL_TEST_IT(ctx, "accepts integers, decimals and exponents");
  {
    const char* yes[] = { "3", "-3", "+3", "3.14", ".5", "-.5", "3.", "3e4", "3e-4", "1.5E+10", "-2e3", "6.02e23" };
    for (size_t i = 0; i < KL_ARRAY_LEN(yes); i++) {
      if (!kl_str_const_is_numeric(KL_STR_CONST(yes[i]))) {
        fprintf(stderr, "[string test] expected \"%s\" to be numeric\n", yes[i]);
        KL_TEST_FAILED();
      }
    }
  }
  KL_TEST_IT_END(ctx);

  KL_TEST_IT(ctx, "rejects a malformed mantissa or exponent");
  {
    const char* no[] = { "", "-", ".", "-.", "e5", ".e5", "1e", "1e-", "1e+", "1e5.0", "3e+-1", "1.2.3", "1x", "abc", "1e5e5" };
    for (size_t i = 0; i < KL_ARRAY_LEN(no); i++) {
      if (kl_str_const_is_numeric(KL_STR_CONST(no[i]))) {
        fprintf(stderr, "[string test] expected \"%s\" not to be numeric\n", no[i]);
        KL_TEST_FAILED();
      }
    }
  }
  KL_TEST_IT_END(ctx);
}
