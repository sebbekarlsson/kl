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
bool kl_cli_looks_like_flag(KL_StrConst arg) {
  if (!kl_str_const_starts_with(arg, KL_STR_CONST_LIT("-"))) return false;
  if (arg.length < 2) return true;
  char c = arg.value[1];
  return !((c >= '0' && c <= '9') || c == '.');
}

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
  KL_StrConst found = {0};
  if (kl_cli_find_arg(argc, (const char**)argv, key, &found)) return found;

  if (required) {
    fprintf(stderr, "Missing required argument: %s\n", key.value);
    exit(1);
  }

  return (KL_StrConst){0};
}


bool kl_cli_find_arg(int argc, const char* argv[], KL_StrConst key, KL_StrConst* out) {
  return kl_cli_find_arg_from(argc, argv, key, 0, out) >= 0;
}

int kl_cli_find_arg_from(int argc, const char* argv[], KL_StrConst key, int start, KL_StrConst* out) {
  KL_StrConst ckey = kl_str_const_remove_prefix(key, KL_STR_CONST_LIT("--"));
  ckey = kl_str_const_remove_prefix(ckey, KL_STR_CONST_LIT("-"));

  for (int i = start < 0 ? 0 : start; i < argc; i++) {
    KL_StrConst arg_a = KL_STR_CONST(argv[i]);
    KL_StrConst arg_b = {0};
    if (i < (argc -1)) {
      arg_b = KL_STR_CONST(argv[i+1]);
    }
    /* Only a flag can be a key: a bare token is a value or a positional, so
       `--save chat` must not also switch on a `--chat` option. */
    if (!kl_cli_looks_like_flag(arg_a)) continue;
    arg_a = kl_str_const_remove_prefix(arg_a, KL_STR_CONST_LIT("--"));
    arg_a = kl_str_const_remove_prefix(arg_a, KL_STR_CONST_LIT("-"));

    /* `--key=value` syntax: split on the first '='. */
    int64_t eq = kl_cli_index_of(arg_a, '=');
    if (eq >= 0) {
      KL_StrConst arg_key = kl_str_const_slice(arg_a, 0, eq);
      if (arg_key.value != 0 && arg_key.length > 0 && kl_str_const_eq(arg_key, ckey)) {
        KL_StrConst arg_val = kl_str_const_slice(arg_a, eq + 1, arg_a.length - (eq + 1));
        *out = kl_cli_unquote(arg_val);
        return i;
      }
      continue;
    }

    if (arg_a.value != 0 && arg_a.length > 0 && kl_str_const_eq(arg_a, ckey)) {
      if (
        arg_b.value != 0 &&
        arg_b.length > 0 &&
        !kl_cli_looks_like_flag(arg_b)
      ) {
        *out = arg_b;
        return i;
      }
      *out = arg_a;
      return i;
    }
  }

  return -1;
}

int64_t kl_cli_get_arg_int(int argc, char* argv[], KL_StrConst key, int64_t fallback) {
  KL_StrConst arg = kl_cli_get_arg(argc, argv, key, false);
  if (arg.value == 0) return fallback;
  return atoll(arg.value);
}

bool kl_cli_value_is_true(KL_StrConst value) {
  return !(kl_str_const_eq(value, KL_STR_CONST_LIT("false")) ||
           kl_str_const_eq(value, KL_STR_CONST_LIT("FALSE")) ||
           kl_str_const_eq(value, KL_STR_CONST_LIT("False")) ||
           kl_str_const_eq(value, KL_STR_CONST_LIT("0")) ||
           kl_str_const_eq(value, KL_STR_CONST_LIT("no")) ||
           kl_str_const_eq(value, KL_STR_CONST_LIT("off")));
}

bool kl_cli_get_arg_bool(int argc, char* argv[], KL_StrConst key, bool fallback) {
  KL_StrConst arg = {0};
  if (!kl_cli_find_arg(argc, (const char**)argv, key, &arg)) return fallback;
  return kl_cli_value_is_true(arg);
}
