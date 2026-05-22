#ifndef KL_CLI_ARGS_H
#define KL_CLI_ARGS_H

#include <kl/string/string.h>
#include <stdbool.h>

KL_StrConst kl_cli_get_arg(int argc, char* argv[], KL_StrConst key, bool required);

#endif
