#include <kl/cli/args.h>
#include <kl/string/string.h>
#include <stdio.h>
#include <stdlib.h>

static int64_t kl_cli_index_of(KL_StrConst str, char c) {
  if (str.value == 0) return -1;
  for (int64_t i = 0; i < str.length; i++) {
    if (str.value[i] == c) return i;
  }
  return -1;
}

/* Strip one pair of matching surrounding quotes, e.g. "bar" -> bar.
   Shells usually strip these themselves; this only matters when the
   quotes reach argv literally (escaped/nested quoting). */
static KL_StrConst kl_cli_unquote(KL_StrConst str) {
  if (str.value == 0 || str.length < 2) return str;
  char first = str.value[0];
  char last = str.value[str.length - 1];
  if ((first == '"' || first == '\'') && first == last) {
    return kl_str_const_slice(str, 1, str.length - 2);
  }
  return str;
}

KL_StrConst kl_cli_get_arg(int argc, char* argv[], KL_StrConst key, bool required) {
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

    /* `--key=value` syntax: split on the first '='. */
    int64_t eq = kl_cli_index_of(arg_a, '=');
    if (eq >= 0) {
      KL_StrConst arg_key = kl_str_const_slice(arg_a, 0, eq);
      if (arg_key.value != 0 && arg_key.length > 0 && kl_str_const_eq(arg_key, ckey)) {
        KL_StrConst arg_val = kl_str_const_slice(arg_a, eq + 1, arg_a.length - (eq + 1));
        return kl_cli_unquote(arg_val);
      }
      continue;
    }

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

int64_t kl_cli_get_arg_int(int argc, char* argv[], KL_StrConst key, int64_t fallback) {
  KL_StrConst arg = kl_cli_get_arg(argc, argv, key, false);
  if (arg.value == 0) return fallback;
  return atoll(arg.value);
}
