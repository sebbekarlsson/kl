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

/* ---- cli program positional-argument test helpers ---- */

typedef struct {
  bool called;
  KL_StrConst from;
  KL_StrConst to;
  bool force;
  int64_t sum;
  int64_t positional_count;
  KL_CLIArgParsed extra;
} KL_CLITestCapture;

static int kl_cli_test_copy_fn(KL_CLICommand* cmd) {
  KL_CLITestCapture* cap = (KL_CLITestCapture*)cmd->config.user_ptr;
  cap->called = true;
  cap->from = kl_cli_command_get_required_string(cmd, KL_STR_CONST_LIT("from"));
  cap->to = kl_cli_command_get_string(cmd, KL_STR_CONST_LIT("to"), (KL_StrConst){0});
  cap->force = kl_cli_command_get_boolean(cmd, KL_STR_CONST_LIT("force"), false);
  cap->positional_count = kl_cli_command_positional_count(cmd);
  return 0;
}

static int kl_cli_test_add_fn(KL_CLICommand* cmd) {
  KL_CLITestCapture* cap = (KL_CLITestCapture*)cmd->config.user_ptr;
  cap->called = true;
  cap->sum = kl_cli_command_get_required_integer(cmd, KL_STR_CONST_LIT("a")) +
             kl_cli_command_get_required_integer(cmd, KL_STR_CONST_LIT("b"));
  cap->positional_count = kl_cli_command_positional_count(cmd);
  cap->extra = kl_cli_command_get_positional(cmd, 2);
  return 0;
}

static KL_CLICommand* kl_cli_test_define_copy(KL_CLIProgram* prog, KL_CLITestCapture* cap) {
  KL_CLICommand* cmd = kl_cli_program_define_command(prog, (KL_CLICommandConfig){
    .name = KL_STR_CONST_LIT("copy"),
    .description = KL_STR_CONST_LIT("Copy a file"),
    .fn = kl_cli_test_copy_fn,
    .user_ptr = cap
  });
  kl_cli_command_add_positional(cmd, (KL_CLIArgConfig){
    .name = KL_STR_CONST_LIT("from"),
    .description = KL_STR_CONST_LIT("Source path"),
    .type = KL_CLI_ARG_STRING,
    .required = true
  });
  kl_cli_command_add_positional(cmd, (KL_CLIArgConfig){
    .name = KL_STR_CONST_LIT("to"),
    .description = KL_STR_CONST_LIT("Destination path"),
    .type = KL_CLI_ARG_STRING,
    .default_value = {
      .type = KL_CLI_ARG_STRING,
      .as.string = KL_STR_CONST_LIT("out.txt")
    }
  });
  kl_cli_command_add_option(cmd, (KL_CLIArgConfig){
    .name = KL_STR_CONST_LIT("force"),
    .description = KL_STR_CONST_LIT("Overwrite the destination"),
    .type = KL_CLI_ARG_BOOLEAN
  });
  return cmd;
}

void test_cli_program_positionals(KL_TestFunctionContext* ctx) {
  KL_TEST_IT(ctx, "matches bare tokens after the command name in declaration order");
  {
    KL_CLIProgram prog = {0};
    kl_cli_program_init(&prog, (KL_CLIProgramConfig){0});

    KL_CLITestCapture cap = {0};
    kl_cli_test_define_copy(&prog, &cap);

    char* argv[] = {"prog", "copy", "a.txt", "b.txt", "--force"};
    int rc = kl_cli_program_run(&prog, (int)KL_ARRAY_LEN(argv), argv);

    KL_TEST_EXPECT_EQ_INT(rc, 0);
    KL_TEST_EXPECT_EXPR(cap.called);
    expect_arg(cap.from, "a.txt");
    expect_arg(cap.to, "b.txt");
    KL_TEST_EXPECT_EXPR(cap.force);
    KL_TEST_EXPECT_EQ_INT(cap.positional_count, 2);

    kl_cli_program_destroy(&prog);
  }
  KL_TEST_IT_END(ctx);

  KL_TEST_IT(ctx, "skips `--key=value` option tokens when collecting positionals");
  {
    KL_CLIProgram prog = {0};
    kl_cli_program_init(&prog, (KL_CLIProgramConfig){0});

    KL_CLITestCapture cap = {0};
    kl_cli_test_define_copy(&prog, &cap);

    char* argv[] = {"prog", "copy", "--force=true", "a.txt", "b.txt"};
    int rc = kl_cli_program_run(&prog, (int)KL_ARRAY_LEN(argv), argv);

    KL_TEST_EXPECT_EQ_INT(rc, 0);
    expect_arg(cap.from, "a.txt");
    expect_arg(cap.to, "b.txt");
    KL_TEST_EXPECT_EXPR(cap.force);

    kl_cli_program_destroy(&prog);
  }
  KL_TEST_IT_END(ctx);
}

void test_cli_program_positional_defaults_and_required(KL_TestFunctionContext* ctx) {
  KL_TEST_IT(ctx, "applies the default of a trailing optional positional");
  {
    KL_CLIProgram prog = {0};
    kl_cli_program_init(&prog, (KL_CLIProgramConfig){0});

    KL_CLITestCapture cap = {0};
    kl_cli_test_define_copy(&prog, &cap);

    char* argv[] = {"prog", "copy", "a.txt"};
    int rc = kl_cli_program_run(&prog, (int)KL_ARRAY_LEN(argv), argv);

    KL_TEST_EXPECT_EQ_INT(rc, 0);
    expect_arg(cap.from, "a.txt");
    expect_arg(cap.to, "out.txt");
    KL_TEST_EXPECT_EXPR(!cap.force);
    KL_TEST_EXPECT_EQ_INT(cap.positional_count, 2);

    kl_cli_program_destroy(&prog);
  }
  KL_TEST_IT_END(ctx);

  KL_TEST_IT(ctx, "fails the run when a required positional is missing");
  {
    KL_CLIProgram prog = {0};
    kl_cli_program_init(&prog, (KL_CLIProgramConfig){0});

    KL_CLITestCapture cap = {0};
    kl_cli_test_define_copy(&prog, &cap);

    char* argv[] = {"prog", "copy"};
    int rc = kl_cli_program_run(&prog, (int)KL_ARRAY_LEN(argv), argv);

    KL_TEST_EXPECT_EXPR(rc != 0);
    KL_TEST_EXPECT_EXPR(!cap.called);

    kl_cli_program_destroy(&prog);
  }
  KL_TEST_IT_END(ctx);
}

void test_cli_program_typed_and_extra_positionals(KL_TestFunctionContext* ctx) {
  KL_TEST_IT(ctx, "parses typed positionals and keeps undeclared extras as strings");
  {
    KL_CLIProgram prog = {0};
    kl_cli_program_init(&prog, (KL_CLIProgramConfig){0});

    KL_CLITestCapture cap = {0};
    KL_CLICommand* cmd = kl_cli_program_define_command(&prog, (KL_CLICommandConfig){
      .name = KL_STR_CONST_LIT("add"),
      .fn = kl_cli_test_add_fn,
      .user_ptr = &cap
    });
    kl_cli_command_add_positional(cmd, (KL_CLIArgConfig){
      .name = KL_STR_CONST_LIT("a"),
      .type = KL_CLI_ARG_INTEGER,
      .required = true
    });
    kl_cli_command_add_positional(cmd, (KL_CLIArgConfig){
      .name = KL_STR_CONST_LIT("b"),
      .type = KL_CLI_ARG_INTEGER,
      .required = true
    });

    char* argv[] = {"prog", "add", "2", "3", "extra"};
    int rc = kl_cli_program_run(&prog, (int)KL_ARRAY_LEN(argv), argv);

    KL_TEST_EXPECT_EQ_INT(rc, 0);
    KL_TEST_EXPECT_EQ_INT(cap.sum, 5);
    KL_TEST_EXPECT_EQ_INT(cap.positional_count, 3);
    KL_TEST_EXPECT_EXPR(cap.extra.type == KL_CLI_ARG_STRING);
    expect_arg(cap.extra.as.string, "extra");

    kl_cli_program_destroy(&prog);
  }
  KL_TEST_IT_END(ctx);

  KL_TEST_IT(ctx, "fails the run when a positional does not parse as its type");
  {
    KL_CLIProgram prog = {0};
    kl_cli_program_init(&prog, (KL_CLIProgramConfig){0});

    KL_CLITestCapture cap = {0};
    KL_CLICommand* cmd = kl_cli_program_define_command(&prog, (KL_CLICommandConfig){
      .name = KL_STR_CONST_LIT("add"),
      .fn = kl_cli_test_add_fn,
      .user_ptr = &cap
    });
    kl_cli_command_add_positional(cmd, (KL_CLIArgConfig){
      .name = KL_STR_CONST_LIT("a"),
      .type = KL_CLI_ARG_INTEGER,
      .required = true
    });

    char* argv[] = {"prog", "add", "not-a-number"};
    int rc = kl_cli_program_run(&prog, (int)KL_ARRAY_LEN(argv), argv);

    KL_TEST_EXPECT_EXPR(rc != 0);
    KL_TEST_EXPECT_EXPR(!cap.called);

    kl_cli_program_destroy(&prog);
  }
  KL_TEST_IT_END(ctx);
}

/* ---- deferred (declared inside the command fn) options ---- */

static int kl_cli_test_deferred_fn(KL_CLICommand* cmd) {
  KL_CLITestCapture* cap = (KL_CLITestCapture*)cmd->config.user_ptr;
  cap->called = true;

  // Declared only now, after kl_cli_program_run's parse loop already ran:
  // add_option must parse it against the stored command line immediately.
  kl_cli_command_add_option(cmd, (KL_CLIArgConfig){
    .name = KL_STR_CONST_LIT("foo"),
    .type = KL_CLI_ARG_INTEGER
  });
  cap->sum = kl_cli_command_get_integer(cmd, KL_STR_CONST_LIT("foo"), -1);

  kl_cli_command_add_option(cmd, (KL_CLIArgConfig){
    .name = KL_STR_CONST_LIT("bar"),
    .type = KL_CLI_ARG_STRING,
    .default_value = {
      .type = KL_CLI_ARG_STRING,
      .as.string = KL_STR_CONST_LIT("fallback")
    }
  });
  cap->from = kl_cli_command_get_string(cmd, KL_STR_CONST_LIT("bar"), (KL_StrConst){0});

  return 0;
}

void test_cli_program_deferred_options(KL_TestFunctionContext* ctx) {
  KL_TEST_IT(ctx, "parses an option declared inside the command function");
  {
    KL_CLIProgram prog = {0};
    kl_cli_program_init(&prog, (KL_CLIProgramConfig){0});

    KL_CLITestCapture cap = {0};
    kl_cli_program_define_command(&prog, (KL_CLICommandConfig){
      .name = KL_STR_CONST_LIT("run"),
      .fn = kl_cli_test_deferred_fn,
      .user_ptr = &cap
    });

    char* argv[] = {"prog", "run", "--foo=123"};
    int rc = kl_cli_program_run(&prog, (int)KL_ARRAY_LEN(argv), argv);

    KL_TEST_EXPECT_EQ_INT(rc, 0);
    KL_TEST_EXPECT_EXPR(cap.called);
    KL_TEST_EXPECT_EQ_INT(cap.sum, 123);

    kl_cli_program_destroy(&prog);
  }
  KL_TEST_IT_END(ctx);

  KL_TEST_IT(ctx, "applies the default of an absent deferred option");
  {
    KL_CLIProgram prog = {0};
    kl_cli_program_init(&prog, (KL_CLIProgramConfig){0});

    KL_CLITestCapture cap = {0};
    kl_cli_program_define_command(&prog, (KL_CLICommandConfig){
      .name = KL_STR_CONST_LIT("run"),
      .fn = kl_cli_test_deferred_fn,
      .user_ptr = &cap
    });

    char* argv[] = {"prog", "run"};
    int rc = kl_cli_program_run(&prog, (int)KL_ARRAY_LEN(argv), argv);

    KL_TEST_EXPECT_EQ_INT(rc, 0);
    KL_TEST_EXPECT_EQ_INT(cap.sum, -1);
    expect_arg(cap.from, "fallback");

    kl_cli_program_destroy(&prog);
  }
  KL_TEST_IT_END(ctx);
}

/* ---- --help dispatch + kl_cli_command_terminate_if_help ---- */

static int kl_cli_test_help_dispatch_fn(KL_CLICommand* cmd) {
  KL_CLITestCapture* cap = (KL_CLITestCapture*)cmd->config.user_ptr;
  cap->called = true;
  cap->force = cmd->help_requested;

  // Deferred option declared under help mode: must register (for the help
  // listing) without exploding, then be readable via its default.
  kl_cli_command_add_option(cmd, (KL_CLIArgConfig){
    .name = KL_STR_CONST_LIT("foo"),
    .type = KL_CLI_ARG_INTEGER,
    .default_value = { .type = KL_CLI_ARG_INTEGER, .as.integer = 7 }
  });
  cap->sum = kl_cli_command_get_integer(cmd, KL_STR_CONST_LIT("foo"), -1);

  // A no-op when --help was not passed; exit(0)s the runner (test failure by
  // vanishing) if the flag were misdetected.
  if (!cmd->help_requested) {
    kl_cli_command_terminate_if_help(cmd);
  }
  return 0;
}

void test_cli_program_help_dispatches_command(KL_TestFunctionContext* ctx) {
  KL_TEST_IT(ctx, "dispatches the named command even when --help is present");
  {
    KL_CLIProgram prog = {0};
    kl_cli_program_init(&prog, (KL_CLIProgramConfig){0});

    KL_CLITestCapture cap = {0};
    KL_CLICommand* cmd = kl_cli_program_define_command(&prog, (KL_CLICommandConfig){
      .name = KL_STR_CONST_LIT("run"),
      .fn = kl_cli_test_help_dispatch_fn,
      .user_ptr = &cap
    });
    // Required args must not block the help dispatch (lenient parse).
    kl_cli_command_add_positional(cmd, (KL_CLIArgConfig){
      .name = KL_STR_CONST_LIT("target"),
      .type = KL_CLI_ARG_STRING,
      .required = true
    });
    kl_cli_command_add_option(cmd, (KL_CLIArgConfig){
      .name = KL_STR_CONST_LIT("count"),
      .type = KL_CLI_ARG_INTEGER,
      .required = true
    });

    char* argv[] = {"prog", "run", "--help"};
    int rc = kl_cli_program_run(&prog, (int)KL_ARRAY_LEN(argv), argv);

    KL_TEST_EXPECT_EQ_INT(rc, 0);
    KL_TEST_EXPECT_EXPR(cap.called);
    KL_TEST_EXPECT_EXPR(cap.force);          /* help_requested was set */
    KL_TEST_EXPECT_EQ_INT(cap.sum, 7);       /* deferred default applied */

    kl_cli_program_destroy(&prog);
  }
  KL_TEST_IT_END(ctx);

  KL_TEST_IT(ctx, "help_requested stays false (and terminate is a no-op) without --help");
  {
    KL_CLIProgram prog = {0};
    kl_cli_program_init(&prog, (KL_CLIProgramConfig){0});

    KL_CLITestCapture cap = {0};
    kl_cli_program_define_command(&prog, (KL_CLICommandConfig){
      .name = KL_STR_CONST_LIT("run"),
      .fn = kl_cli_test_help_dispatch_fn,
      .user_ptr = &cap
    });

    char* argv[] = {"prog", "run"};
    int rc = kl_cli_program_run(&prog, (int)KL_ARRAY_LEN(argv), argv);

    KL_TEST_EXPECT_EQ_INT(rc, 0);
    KL_TEST_EXPECT_EXPR(cap.called);
    KL_TEST_EXPECT_EXPR(!cap.force);

    kl_cli_program_destroy(&prog);
  }
  KL_TEST_IT_END(ctx);

  KL_TEST_IT(ctx, "bare --help / unknown command with --help prints program help, rc 0");
  {
    KL_CLIProgram prog = {0};
    kl_cli_program_init(&prog, (KL_CLIProgramConfig){0});

    KL_CLITestCapture cap = {0};
    kl_cli_program_define_command(&prog, (KL_CLICommandConfig){
      .name = KL_STR_CONST_LIT("run"),
      .fn = kl_cli_test_help_dispatch_fn,
      .user_ptr = &cap
    });

    char* argv_bare[] = {"prog", "--help"};
    KL_TEST_EXPECT_EQ_INT(kl_cli_program_run(&prog, (int)KL_ARRAY_LEN(argv_bare), argv_bare), 0);
    KL_TEST_EXPECT_EXPR(!cap.called);

    char* argv_unknown[] = {"prog", "nope", "-h"};
    KL_TEST_EXPECT_EQ_INT(kl_cli_program_run(&prog, (int)KL_ARRAY_LEN(argv_unknown), argv_unknown), 0);
    KL_TEST_EXPECT_EXPR(!cap.called);

    kl_cli_program_destroy(&prog);
  }
  KL_TEST_IT_END(ctx);
}
