#ifndef KL_CLI_ARGS_H
#define KL_CLI_ARGS_H

#include <kl/string/string.h>
#include <stdbool.h>
#include <stdint.h>


bool kl_cli_find_arg(int argc, const char* argv[], KL_StrConst key, KL_StrConst* out);

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
