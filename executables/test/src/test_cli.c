#include <test/tests.h>

/* ---- cli args test helpers ---- */

/* Compare a returned KL_StrConst against an expected C-string by length.
   kl_cli_get_arg does not guarantee a NUL-terminated result (a value
   sliced out of a `--key="value"` token points into the middle of argv),
   so matching is always length-aware and never relies on `.value` being
   a valid C-string. */
static void expect_arg(KL_StrConst got, const char* expected) {
  int64_t exp_len = (int64_t)strlen(expected);
  bool eq;
  if (exp_len == 0) {
    eq = got.length == 0;
  } else {
    eq = got.length == exp_len && got.value != 0 &&
         memcmp(got.value, expected, (size_t)exp_len) == 0;
  }
  if (!eq) {
    fprintf(stderr, "[cli test] expected \"%s\" but got \"%.*s\"\n",
            expected, (int)got.length, got.value != 0 ? got.value : "");
    KL_TEST_FAILED();
  }
}

void test_cli_get_arg_space_syntax(KL_TestFunctionContext* ctx) {
  KL_TEST_IT(ctx, "reads `--key value` and bare flags");
  {
    char* argv[] = {"prog", "--name", "alice", "--verbose"};
    int argc = (int)KL_ARRAY_LEN(argv);

    /* `--key value` returns the following token. */
    expect_arg(kl_cli_get_arg(argc, argv, KL_STR_CONST_LIT("--name"), false), "alice");

    /* a single-dash key form resolves the same. */
    expect_arg(kl_cli_get_arg(argc, argv, KL_STR_CONST_LIT("-name"), false), "alice");

    /* a bare flag with no value returns the key itself as a presence marker. */
    expect_arg(kl_cli_get_arg(argc, argv, KL_STR_CONST_LIT("--verbose"), false), "verbose");
  }
  KL_TEST_IT_END(ctx);

  KL_TEST_IT(ctx, "returns an empty result for a missing, non-required key");
  {
    char* argv[] = {"prog", "--name", "alice"};
    int argc = (int)KL_ARRAY_LEN(argv);

    KL_StrConst got = kl_cli_get_arg(argc, argv, KL_STR_CONST_LIT("--missing"), false);
    KL_TEST_EXPECT_EXPR(got.value == 0);
    KL_TEST_EXPECT_EXPR(got.length == 0);
  }
  KL_TEST_IT_END(ctx);
}

void test_cli_get_arg_equals_syntax(KL_TestFunctionContext* ctx) {
  KL_TEST_IT(ctx, "splits `--key=value` on the first '='");
  {
    char* argv[] = {"prog", "--name=alice", "--age=30"};
    int argc = (int)KL_ARRAY_LEN(argv);

    expect_arg(kl_cli_get_arg(argc, argv, KL_STR_CONST_LIT("--name"), false), "alice");
    expect_arg(kl_cli_get_arg(argc, argv, KL_STR_CONST_LIT("--age"), false), "30");

    /* single-dash key form. */
    char* argv2[] = {"prog", "-name=bob"};
    expect_arg(kl_cli_get_arg((int)KL_ARRAY_LEN(argv2), argv2,
                              KL_STR_CONST_LIT("--name"), false), "bob");
  }
  KL_TEST_IT_END(ctx);

  KL_TEST_IT(ctx, "keeps later '=' characters inside the value");
  {
    char* argv[] = {"prog", "--expr=a=b=c"};
    expect_arg(kl_cli_get_arg((int)KL_ARRAY_LEN(argv), argv,
                              KL_STR_CONST_LIT("--expr"), false), "a=b=c");
  }
  KL_TEST_IT_END(ctx);

  KL_TEST_IT(ctx, "treats `--key=` as an empty value");
  {
    char* argv[] = {"prog", "--name="};
    KL_StrConst got = kl_cli_get_arg((int)KL_ARRAY_LEN(argv), argv,
                                     KL_STR_CONST_LIT("--name"), false);
    KL_TEST_EXPECT_EXPR(got.length == 0);
  }
  KL_TEST_IT_END(ctx);

  KL_TEST_IT(ctx, "skips a `--key=value` token whose key differs");
  {
    char* argv[] = {"prog", "--other=x", "--name=alice"};
    int argc = (int)KL_ARRAY_LEN(argv);

    expect_arg(kl_cli_get_arg(argc, argv, KL_STR_CONST_LIT("--name"), false), "alice");

    KL_StrConst got = kl_cli_get_arg(argc, argv, KL_STR_CONST_LIT("--missing"), false);
    KL_TEST_EXPECT_EXPR(got.value == 0);
  }
  KL_TEST_IT_END(ctx);
}

void test_cli_get_arg_quoted_values(KL_TestFunctionContext* ctx) {
  KL_TEST_IT(ctx, "strips one pair of matching surrounding quotes");
  {
    char* dq[] = {"prog", "--name=\"alice\""};
    expect_arg(kl_cli_get_arg((int)KL_ARRAY_LEN(dq), dq,
                              KL_STR_CONST_LIT("--name"), false), "alice");

    char* sq[] = {"prog", "--name='alice'"};
    expect_arg(kl_cli_get_arg((int)KL_ARRAY_LEN(sq), sq,
                              KL_STR_CONST_LIT("--name"), false), "alice");
  }
  KL_TEST_IT_END(ctx);

  KL_TEST_IT(ctx, "leaves mismatched or inner quotes untouched");
  {
    char* mismatch[] = {"prog", "--name=\"alice'"};
    expect_arg(kl_cli_get_arg((int)KL_ARRAY_LEN(mismatch), mismatch,
                              KL_STR_CONST_LIT("--name"), false), "\"alice'");

    char* inner[] = {"prog", "--msg=say \"hi\""};
    expect_arg(kl_cli_get_arg((int)KL_ARRAY_LEN(inner), inner,
                              KL_STR_CONST_LIT("--msg"), false), "say \"hi\"");
  }
  KL_TEST_IT_END(ctx);
}
