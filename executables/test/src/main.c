#include <stdio.h>
#include <test/framework.h>
#include <kl/macros.h>
#include <kl/string/string.h>

static KL_StrConst get_arg(int argc, char* argv[], KL_StrConst key, bool required) {
  KL_StrConst ckey = kl_str_const_remove_prefix(key, KL_STR_CONST_LIT("--"));
  ckey = kl_str_const_remove_prefix(ckey, KL_STR_CONST_LIT("-"));

  for (int i = 0; i < argc; i++) {
    KL_StrConst arg_a = KL_STR_CONST(argv[i]);
    KL_StrConst arg_b = {0};
    if (i < (argc -1)) {
      arg_b = KL_STR_CONST(argv[i+1]);
    }
    arg_a = kl_str_const_remove_prefix(arg_a, KL_STR_CONST_LIT("--"));
    arg_a = kl_str_const_remove_prefix(arg_a, KL_STR_CONST_LIT("-"));
    if (arg_a.value != 0 && arg_a.length > 0 && kl_str_const_eq(arg_a, ckey)) {
      if (
        arg_b.value != 0 &&
        arg_b.length > 0 &&
        !kl_str_const_starts_with(arg_b, KL_STR_CONST_LIT("-"))
      ) {
        return arg_b;
      }
      return arg_a;
    }
  }

  if (required) {
    fprintf(stderr, "Missing required argument: %s\n", key.value);
    exit(1);
  }

  return (KL_StrConst){0};
}





static void example_test0() {}
static void example_test1() {}
static void example_test2() {}


static void run_test(KL_TestEntry* entry) {
  KL_TEST_BEGIN(entry->name);
  entry->fn();
  KL_TEST_END(entry->name);
}

int main(int argc, char* argv[]) {
  KL_TestEntry tests[] = {
    KL_TEST_ENTRY(example_test0),
    KL_TEST_ENTRY(example_test1),
    KL_TEST_ENTRY(example_test2),
  };

  int64_t n_tests = KL_ARRAY_LEN(tests);

  KL_StrConst test_to_run = get_arg(argc, argv, KL_STR_CONST_LIT("--test"), false);

  if (test_to_run.value != 0) {
    for (int64_t i = 0; i < n_tests; i++) {
      KL_TestEntry* entry = &tests[i];
      if (kl_str_const_eq(test_to_run, KL_STR_CONST(entry->name))) {
        run_test(entry);
        break;
      }
    }
    fprintf(stderr, "No such test: '%s'\n", test_to_run.value);
    exit(1);
  } else {
    for (int64_t i = 0; i < n_tests; i++) {
      KL_TestEntry* entry = &tests[i];
      run_test(entry);
    }
  }
  
  
  return 0;
}
