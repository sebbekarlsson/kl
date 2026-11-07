#ifndef KL_CLI_ARGS_H
#define KL_CLI_ARGS_H

#include <kl/string/string.h>
#include <stdbool.h>
#include <stdint.h>


/* True if `arg` is spelt like an option (`-x`, `--name`, `--name=v`) rather
   than a value. A value that starts with '-' is the next flag, not this key's
   value -- unless it is a negative number ("-45.5", "-.5"): no flag is spelt
   like that, and `--pos -45.5,2,3` has to be able to carry one. */
bool kl_cli_looks_like_flag(KL_StrConst arg);

/* The first occurrence of `key` (`--key value`, `--key=value`, or a bare
   `--key`, whose value is the key itself). Only flag-like tokens are matched,
   so a value that happens to spell an option's name is not that option. */
bool kl_cli_find_arg(int argc, const char* argv[], KL_StrConst key, KL_StrConst* out);

/* kl_cli_find_arg starting at argv[start]: returns the index of the matching
   key token, or -1. Calling it again from that index + 1 walks every
   occurrence of a repeated option. */
int kl_cli_find_arg_from(int argc, const char* argv[], KL_StrConst key, int start, KL_StrConst* out);

KL_StrConst kl_cli_get_arg(int argc, char* argv[], KL_StrConst key, bool required);

int64_t kl_cli_get_arg_int(int argc, char* argv[], KL_StrConst key, int64_t fallback);

/* A boolean flag's value: anything but an explicit falsey value
   (false / FALSE / False / 0 / no / off) is true, so a bare `--flag`
   (whose value is the key itself) counts as true. */
bool kl_cli_value_is_true(KL_StrConst value);

/* `fallback` when the flag is absent, else kl_cli_value_is_true of its value:
   `--flag`, `--flag=1` are true, `--flag=false`, `--flag 0` false. */
bool kl_cli_get_arg_bool(int argc, char* argv[], KL_StrConst key, bool fallback);

#endif
