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
  // Shown next to the argument in the `--help` output.
  KL_StrConst description;
  KL_CLIArgType type;
  bool required;

  // An option that may be given more than once (`--exclude a --exclude b`):
  // every occurrence is kept, in command-line order, and read back with
  // kl_cli_command_get_arg_count / kl_cli_command_get_arg_at. A plain
  // option's later occurrences are ignored. Options only, not positionals.
  bool repeated;

  // Optional fallback used when the argument is absent. A non-UNDEFINED
  // `type` marks the default as set; its `type` must match `type` above.
  KL_CLIArgParsed default_value;
} KL_CLIArgConfig;

KL_DEFINE_LIST(cli_arg_config, KL_CLIArgConfig);


typedef struct {
  KL_StrConst name;
  // Shown next to the command in the `--help` output.
  KL_StrConst description;
  KL_CLICommandFunction fn;
  void* user_ptr;
} KL_CLICommandConfig;


struct KL_CLICommand {
  bool initialized;
  KL_CLICommandConfig config;
  KL_cli_arg_config_List config_args;
  KL_cli_arg_parsed_List parsed_args;
  KL_cli_arg_config_List config_positionals;
  KL_cli_arg_parsed_List parsed_positionals;
  KL_CLIProgram* program;
  // The command line the command was dispatched with; set by
  // kl_cli_program_run just before parsing, so options declared *inside* the
  // command function ("deferred" options) can still be parsed against it.
  // Zero until the command is actually run.
  int argc;
  char** argv;
  // True when `--help`/`-h` was present on the command line. The command is
  // dispatched anyway (so it can declare its deferred options); it decides
  // when to stop — see kl_cli_command_terminate_if_help.
  bool help_requested;
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

// Parse the command line and dispatch the named command. `--help`/`-h` with
// no (or an unknown) command prints the program-level help and returns 0.
// With a matching command, the command function still runs — arguments are
// parsed leniently (a missing required or invalid value does not abort) so
// the function can declare its deferred options first; it is expected to call
// kl_cli_command_terminate_if_help once they are all declared.
int kl_cli_program_run(KL_CLIProgram* program, int argc, char* argv[]);

KL_CLICommand* kl_cli_program_define_command(KL_CLIProgram* program, KL_CLICommandConfig cfg);

// -------- command

// Declare an option (`--name value` / `--name=value`). A BOOLEAN option is
// true when given bare (`--force`); a value, when it has one, must be a
// boolean word -- true/false, 1/0, yes/no, on/off -- so `--force out.txt`,
// which would otherwise swallow a positional, is an error rather than "true". Normally called while
// building the program, before kl_cli_program_run parses the command line —
// but it may also be called from inside the command function itself (e.g. a
// sub-mode declaring its own options lazily): an option added after the
// command was dispatched is parsed immediately against the stored command
// line, so kl_cli_command_get_* sees it right away. In that deferred case an
// invalid supplied value or a missing required option prints the same error
// the up-front parse would have and exits(1).
KL_CLICommand* kl_cli_command_add_option(KL_CLICommand* cmd, KL_CLIArgConfig cfg);

// If `--help`/`-h` was on the command line, print this command's help
// (usage, positionals, options — including any declared after dispatch) to
// stdout and exit(0); otherwise do nothing. Command functions should call
// this right after declaring their deferred options, before doing real work.
void kl_cli_command_terminate_if_help(KL_CLICommand* cmd);

// Declare a positional argument. Positionals are matched in declaration order
// against the bare (non `-`-prefixed) tokens that follow the command name,
// e.g. `prog copy <from> <to> --force`. A bare token directly after a
// `--key` token (without `=`) is consumed as that option's value, not as a
// positional — prefer `--key=value` when mixing options between positionals.
// A required positional may not be declared after an optional one.
// Tokens supplied beyond the declared positionals are kept as strings and are
// reachable via kl_cli_command_get_positional.
KL_CLICommand* kl_cli_command_add_positional(KL_CLICommand* cmd, KL_CLIArgConfig cfg);

// Check the command line against what this command declares: every option
// token must name a declared option (or be --help / -h), and there may be no
// more bare tokens than declared positionals. Prints one line to stderr per
// offender and returns false if there were any. Without it, an unknown option
// is silently ignored -- a misspelt `--stpes 10` just runs with the default.
// Call it after declaring any deferred options.
bool kl_cli_command_reject_unknown(KL_CLICommand* cmd);

// Number of positional values available to the command function (supplied
// tokens plus applied defaults).
int64_t kl_cli_command_positional_count(KL_CLICommand* cmd);

// Positional value by its zero-based position. Out-of-range indices return a
// value with type KL_CLI_ARG_UNDEFINED.
KL_CLIArgParsed kl_cli_command_get_positional(KL_CLICommand* cmd, int64_t index);

// Look up a parsed argument by name. Searches options first, then declared
// positionals. If the argument was not
// supplied (and has no default), the returned value has type KL_CLI_ARG_UNDEFINED.
KL_CLIArgParsed kl_cli_command_get_arg(KL_CLICommand* cmd, KL_StrConst name);

// How many values a `repeated` option has (its occurrences, or 1 when only
// its default applies; 0 when absent with no default). For a plain option, 1
// or 0.
int64_t kl_cli_command_get_arg_count(KL_CLICommand* cmd, KL_StrConst name);

// The `index`th value of a `repeated` option, in command-line order. Out of
// range returns a value with type KL_CLI_ARG_UNDEFINED.
KL_CLIArgParsed kl_cli_command_get_arg_at(KL_CLICommand* cmd, KL_StrConst name, int64_t index);

// Typed convenience accessors. Each returns the option's parsed value, or
// `fallback` if the option was not supplied or was parsed as a different type.
KL_StrConst kl_cli_command_get_string(KL_CLICommand* cmd, KL_StrConst name, KL_StrConst fallback);
int64_t     kl_cli_command_get_integer(KL_CLICommand* cmd, KL_StrConst name, int64_t fallback);
bool        kl_cli_command_get_boolean(KL_CLICommand* cmd, KL_StrConst name, bool fallback);
double      kl_cli_command_get_float(KL_CLICommand* cmd, KL_StrConst name, double fallback);

// Accessors for options/positionals declared with `.required = true`. They
// take no fallback: a required argument is guaranteed to be parsed before the
// command function runs. They assert if the named argument is not registered,
// was not declared required, or has a different type.
KL_StrConst kl_cli_command_get_required_string(KL_CLICommand* cmd, KL_StrConst name);
int64_t     kl_cli_command_get_required_integer(KL_CLICommand* cmd, KL_StrConst name);
bool        kl_cli_command_get_required_boolean(KL_CLICommand* cmd, KL_StrConst name);
double      kl_cli_command_get_required_float(KL_CLICommand* cmd, KL_StrConst name);

#endif
