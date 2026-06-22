#ifndef KL_CLI_ARGS_H
#define KL_CLI_ARGS_H

#include <kl/string/string.h>
#include <stdbool.h>
#include <stdint.h>

KL_StrConst kl_cli_get_arg(int argc, char* argv[], KL_StrConst key, bool required);

int64_t kl_cli_get_arg_int(int argc, char* argv[], KL_StrConst key, int64_t fallback);

#endif
