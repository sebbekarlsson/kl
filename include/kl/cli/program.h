#ifndef KL_CLI_PROGRAM_H
#define KL_CLI_PROGRAM_H

#include <kl/string/string.h>
#include <kl/templates/list/list.h>
#include <kl/memory/allocator.h>
#include <kl/templates/sparse_set/binary_sparse_set.h>
#include <stdbool.h>
#include <stdint.h>


typedef struct KL_CLIProgram KL_CLIProgram;
typedef struct KL_CLICommand KL_CLICommand;

typedef int (*KL_CLICommandFunction)(struct KL_CLICommand* cmd);

typedef enum {
  KL_CLI_ARG_UNDEFINED,
  KL_CLI_ARG_STRING,
  KL_CLI_ARG_INTEGER,
  KL_CLI_ARG_BOOLEAN,
  KL_CLI_ARG_FLOAT
} KL_CLIArgType;

typedef struct {
  KL_StrConst name;
  KL_CLIArgType type;

  union {
    KL_StrConst string;
    int64_t integer;
    bool boolean;
    double floating;
  } as;
  
} KL_CLIArgParsed;

KL_DEFINE_LIST(cli_arg_parsed, KL_CLIArgParsed);

typedef struct {
  KL_StrConst name;
  KL_CLIArgType type;
  bool required;

  // Optional fallback used when the argument is absent. A non-UNDEFINED
  // `type` marks the default as set; its `type` must match `type` above.
  KL_CLIArgParsed default_value;
} KL_CLIArgConfig;

KL_DEFINE_LIST(cli_arg_config, KL_CLIArgConfig);


typedef struct {
  KL_StrConst name;
  KL_CLICommandFunction fn;
  void* user_ptr;
} KL_CLICommandConfig;


struct KL_CLICommand {
  bool initialized;
  KL_CLICommandConfig config;
  KL_cli_arg_config_List config_args;
  KL_cli_arg_parsed_List parsed_args;
  KL_CLIProgram* program;
}; 

KL_DEFINE_LIST(cli_command, KL_CLICommand);
KL_DEFINE_LIST(cli_command_ptr, KL_CLICommand*);

typedef struct {
  int64_t allocator_capacity;
} KL_CLIProgramConfig;

struct KL_CLIProgram {
  bool initialized;
  KL_CLIProgramConfig config;
  KL_Allocator allocator;
  struct {
    KL_BinarySparseSet set;
    int64_t id_increment;
  } commands;
};


// -------- program

void kl_cli_program_init(KL_CLIProgram* program, KL_CLIProgramConfig cfg);

void kl_cli_program_destroy(KL_CLIProgram* program);

int kl_cli_program_run(KL_CLIProgram* program, int argc, char* argv[]);

KL_CLICommand* kl_cli_program_define_command(KL_CLIProgram* program, KL_CLICommandConfig cfg);

// -------- command

 KL_CLICommand* kl_cli_command_add_option(KL_CLICommand* cmd, KL_CLIArgConfig cfg);

// Look up a parsed argument by its option name. If the argument was not
// supplied (and has no default), the returned value has type KL_CLI_ARG_UNDEFINED.
KL_CLIArgParsed kl_cli_command_get_arg(KL_CLICommand* cmd, KL_StrConst name);

// Typed convenience accessors. Each returns the option's parsed value, or
// `fallback` if the option was not supplied or was parsed as a different type.
KL_StrConst kl_cli_command_get_string(KL_CLICommand* cmd, KL_StrConst name, KL_StrConst fallback);
int64_t     kl_cli_command_get_integer(KL_CLICommand* cmd, KL_StrConst name, int64_t fallback);
bool        kl_cli_command_get_boolean(KL_CLICommand* cmd, KL_StrConst name, bool fallback);
double      kl_cli_command_get_float(KL_CLICommand* cmd, KL_StrConst name, double fallback);

// Accessors for options declared with `.required = true`. They take no
// fallback: a required option is guaranteed to be parsed before the command
// function runs. They assert if the named option is not registered, was not
// declared required, or has a different type.
KL_StrConst kl_cli_command_get_required_string(KL_CLICommand* cmd, KL_StrConst name);
int64_t     kl_cli_command_get_required_integer(KL_CLICommand* cmd, KL_StrConst name);
bool        kl_cli_command_get_required_boolean(KL_CLICommand* cmd, KL_StrConst name);
double      kl_cli_command_get_required_float(KL_CLICommand* cmd, KL_StrConst name);

#endif
