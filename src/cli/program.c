#include <kl/cli/program.h>
#include <kl/cli/args.h>
#include <kl/macros.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <ctype.h>
#include <stddef.h>

KL_IMPL_LIST(cli_arg_parsed, KL_CLIArgParsed);
KL_IMPL_LIST(cli_arg_config, KL_CLIArgConfig);
KL_IMPL_LIST(cli_command, KL_CLICommand);
KL_IMPL_LIST(cli_command_ptr, KL_CLICommand*);

// -------- program

void kl_cli_program_init(KL_CLIProgram* program, KL_CLIProgramConfig cfg) {
  if (program->initialized) return;
  if (cfg.allocator_capacity <= 0) cfg.allocator_capacity = 1024;

  program->config = cfg;
  
  kl_allocator_init(&program->allocator, (KL_AllocatorConfig){
    .capacity = cfg.allocator_capacity
  });

  kl_binary_sparseset_init(&program->commands.set, (KL_BinarySparseSetConfig){
    .sparse_capacity = 32,
    .dense_capacity = 32,
    .element_alignment = alignof(KL_CLICommand),
    .element_size = sizeof(KL_CLICommand),
    .allocator = &program->allocator
  });

  program->commands.id_increment = 0;

  program->initialized = true;
}

void kl_cli_program_destroy(KL_CLIProgram* program) {
  assert(program->initialized);
  kl_allocator_destroy(&program->allocator);
  kl_binary_sparseset_destroy(&program->commands.set);
  memset(program, 0, sizeof(KL_CLIProgram));
}

static inline bool parse_arg(KL_CLICommand* cmd, KL_CLIArgConfig argcfg, int argc, char* argv[], KL_CLIArgParsed* out) {
  // A default value makes the argument optional: never let kl_cli_get_arg
  // exit(1) on a missing required arg when we have a fallback to apply.
  bool has_default = argcfg.default_value.type != KL_CLI_ARG_UNDEFINED;

  KL_StrConst value = kl_cli_get_arg(argc, argv, argcfg.name, argcfg.required && !has_default);

  if (value.length <= 0 || value.value == 0) {
    if (has_default) {
      *out = argcfg.default_value;
      out->name = argcfg.name;
      return true;
    }
    return false;
  }

  out->name = argcfg.name;

  switch (argcfg.type) {
    case KL_CLI_ARG_UNDEFINED: return false;
    case KL_CLI_ARG_STRING: {
      out->type = KL_CLI_ARG_STRING;
      out->as.string = value;
      return true;
    };
    case KL_CLI_ARG_INTEGER: {
      out->type = KL_CLI_ARG_INTEGER;

      if (!kl_str_const_is_integer(value)) return false;

      char tmp[256] = {0};
      // `%.*s` bounds the read by value.length (KL_StrConst is not guaranteed
      // null-terminated) and snprintf bounds the write by sizeof(tmp).
      snprintf(tmp, sizeof(tmp), "%.*s", (int)value.length, value.value);
      out->as.integer = atoll(tmp);

      return true;
    };
    case KL_CLI_ARG_BOOLEAN: {
      out->type = KL_CLI_ARG_BOOLEAN;
      // A boolean flag is true simply by being present: kl_cli_get_arg returns
      // the key name itself for a bare `--flag`. So anything that isn't an
      // explicit falsey value counts as true.
      out->as.boolean = !(kl_str_const_eq(value, KL_STR_CONST_LIT("false")) ||
                          kl_str_const_eq(value, KL_STR_CONST_LIT("FALSE")) ||
                          kl_str_const_eq(value, KL_STR_CONST_LIT("False")) ||
                          kl_str_const_eq(value, KL_STR_CONST_LIT("0")) ||
                          kl_str_const_eq(value, KL_STR_CONST_LIT("no")) ||
                          kl_str_const_eq(value, KL_STR_CONST_LIT("off")));
      return true;
    };
    case KL_CLI_ARG_FLOAT: {
      out->type = KL_CLI_ARG_FLOAT;

      if (!kl_str_const_is_numeric(value)) return false;

      char tmp[256] = {0};
      snprintf(tmp, sizeof(tmp), "%.*s", (int)value.length, value.value);
      out->as.floating = atof(tmp);

      return true;
    };
  }

  return false;
}

static const char* arg_type_name(KL_CLIArgType type) {
  switch (type) {
    case KL_CLI_ARG_STRING:  return "string";
    case KL_CLI_ARG_INTEGER: return "integer";
    case KL_CLI_ARG_BOOLEAN: return "boolean";
    case KL_CLI_ARG_FLOAT:   return "float";
    case KL_CLI_ARG_UNDEFINED:
    default:                 return "undefined";
  }
}

static void print_arg_default(FILE* out, KL_CLIArgParsed def) {
  switch (def.type) {
    case KL_CLI_ARG_STRING:
      fprintf(out, " (default: \"%.*s\")", (int)def.as.string.length, def.as.string.value);
      break;
    case KL_CLI_ARG_INTEGER:
      fprintf(out, " (default: %ld)", def.as.integer);
      break;
    case KL_CLI_ARG_BOOLEAN:
      fprintf(out, " (default: %s)", def.as.boolean ? "true" : "false");
      break;
    case KL_CLI_ARG_FLOAT:
      fprintf(out, " (default: %g)", def.as.floating);
      break;
    case KL_CLI_ARG_UNDEFINED:
    default:
      break;
  }
}

static void print_help(KL_CLIProgram* program, const char* prog_name, FILE* out) {
  fprintf(out, "Usage: %s <command> [options]\n\n", prog_name);
  fprintf(out, "Commands:\n");

  if (program->commands.set.length == 0) {
    fprintf(out, "  (none)\n");
    return;
  }

  for (int64_t i = 0; i < program->commands.set.length; i++) {
    int64_t idx = program->commands.set.indices[i];
    KL_CLICommand* cmd = (KL_CLICommand*)kl_binary_sparseset_get(&program->commands.set, idx);
    assert(cmd != 0);

    fprintf(out, "  %.*s\n", (int)cmd->config.name.length, cmd->config.name.value);

    for (int64_t j = 0; j < cmd->config_args.length; j++) {
      KL_CLIArgConfig arg = cmd->config_args.items[j];
      fprintf(out, "    --%-12.*s %-9s",
              (int)arg.name.length, arg.name.value,
              arg_type_name(arg.type));
      if (arg.required) fprintf(out, " (required)");
      print_arg_default(out, arg.default_value);
      fprintf(out, "\n");
    }
  }
}

int kl_cli_program_run(KL_CLIProgram* program, int argc, char* argv[]) {
  assert(program->initialized);

  const char* prog_name = (argc > 0 && argv[0] != 0) ? argv[0] : "program";

  // `--help` / `-h` anywhere on the command line prints usage and stops.
  for (int i = 1; i < argc; i++) {
    KL_StrConst a = KL_STR_CONST(argv[i]);
    if (kl_str_const_eq(a, KL_STR_CONST_LIT("--help")) ||
        kl_str_const_eq(a, KL_STR_CONST_LIT("-h"))) {
      print_help(program, prog_name, stdout);
      return 0;
    }
  }

  if (argc <= 1) {
    fprintf(stderr, "No command specified.\n\n");
    print_help(program, prog_name, stderr);
    return 1;
  };

  KL_StrConst cmd_name = KL_STR_CONST(argv[1]);


  for (int64_t i = 0; i < program->commands.set.length; i++) {
    int64_t idx = program->commands.set.indices[i];
    KL_CLICommand* cmd = (KL_CLICommand*)kl_binary_sparseset_get(&program->commands.set, idx);
    assert(cmd != 0);
    if (kl_str_const_eq(cmd_name, cmd->config.name)) {
      for (int64_t j = 0; j < cmd->config_args.length; j++) {
        KL_CLIArgConfig arg_cfg = cmd->config_args.items[j];
        KL_CLIArgParsed parsed = {0};
        if (!parse_arg(cmd, arg_cfg, argc, argv, &parsed)) {
          if (!arg_cfg.required) continue;
          
          fprintf(stderr, "Failed to parse argument: '%s'\n", arg_cfg.name.value);
          return 1;
        }
        kl_cli_arg_parsed_list_push(&cmd->parsed_args, parsed);
      }
      return cmd->config.fn(cmd);
    }
  }

  fprintf(stderr, "No such command: '%s'\n\n", cmd_name.value);
  print_help(program, prog_name, stderr);
  return 1;
}

KL_CLICommand* kl_cli_program_define_command(KL_CLIProgram* program, KL_CLICommandConfig cfg) {
  assert(program->initialized);

  assert(cfg.name.value != 0);
  assert(cfg.name.length != 0);
  assert(cfg.fn != 0);
  
  int64_t id = program->commands.id_increment;

  KL_CLICommand* cmd = (KL_CLICommand*)kl_binary_sparseset_set(&program->commands.set, id, 0);
  assert(cmd != 0);
  cmd->program = program;
  cmd->config = cfg;

  kl_cli_arg_config_list_init(&cmd->config_args, (KL_ListConfig){
    .allocator = &program->allocator,
    .capacity = 16
  });

  kl_cli_arg_parsed_list_init(&cmd->parsed_args, (KL_ListConfig){
    .allocator = &program->allocator,
    .capacity = cmd->config_args.config.capacity
  });

  cmd->initialized = true;
  program->commands.id_increment += 1;

  return cmd;
}

// -------- command

KL_CLICommand* kl_cli_command_add_option(KL_CLICommand* cmd, KL_CLIArgConfig cfg) {
  assert(cmd->initialized);
  assert(cmd->program != 0);
  assert(cfg.name.value != 0);
  assert(cfg.name.length > 0);

  // A default value, if given, must match the option's declared type.
  assert(cfg.default_value.type == KL_CLI_ARG_UNDEFINED ||
         cfg.default_value.type == cfg.type);

  kl_cli_arg_config_list_push(&cmd->config_args, cfg);

  return cmd;
}

KL_CLIArgParsed kl_cli_command_get_arg(KL_CLICommand* cmd, KL_StrConst name) {
  assert(cmd != 0);
  assert(cmd->initialized);

  for (int64_t i = 0; i < cmd->parsed_args.length; i++) {
    if (kl_str_const_eq(cmd->parsed_args.items[i].name, name)) {
      return cmd->parsed_args.items[i];
    }
  }

  return (KL_CLIArgParsed){0};
}

KL_StrConst kl_cli_command_get_string(KL_CLICommand* cmd, KL_StrConst name, KL_StrConst fallback) {
  KL_CLIArgParsed arg = kl_cli_command_get_arg(cmd, name);
  return arg.type == KL_CLI_ARG_STRING ? arg.as.string : fallback;
}

int64_t kl_cli_command_get_integer(KL_CLICommand* cmd, KL_StrConst name, int64_t fallback) {
  KL_CLIArgParsed arg = kl_cli_command_get_arg(cmd, name);
  return arg.type == KL_CLI_ARG_INTEGER ? arg.as.integer : fallback;
}

bool kl_cli_command_get_boolean(KL_CLICommand* cmd, KL_StrConst name, bool fallback) {
  KL_CLIArgParsed arg = kl_cli_command_get_arg(cmd, name);
  return arg.type == KL_CLI_ARG_BOOLEAN ? arg.as.boolean : fallback;
}

double kl_cli_command_get_float(KL_CLICommand* cmd, KL_StrConst name, double fallback) {
  KL_CLIArgParsed arg = kl_cli_command_get_arg(cmd, name);
  return arg.type == KL_CLI_ARG_FLOAT ? arg.as.floating : fallback;
}

static KL_CLIArgParsed get_required_arg(KL_CLICommand* cmd, KL_StrConst name, KL_CLIArgType expected) {
  assert(cmd != 0);
  assert(cmd->initialized);

  // The option must be registered, and registered as required: a required
  // accessor has no fallback, so its safety relies on `.required = true`
  // guaranteeing the value is parsed before the command function runs.
  KL_CLIArgConfig* cfg = 0;
  for (int64_t i = 0; i < cmd->config_args.length; i++) {
    if (kl_str_const_eq(cmd->config_args.items[i].name, name)) {
      cfg = &cmd->config_args.items[i];
      break;
    }
  }
  assert(cfg != 0 && "kl_cli_command_get_required_*: no option with this name");
  assert(cfg->required && "kl_cli_command_get_required_*: option was not declared required");
  assert(cfg->type == expected && "kl_cli_command_get_required_*: option type mismatch");

  KL_CLIArgParsed arg = kl_cli_command_get_arg(cmd, name);
  assert(arg.type == expected && "kl_cli_command_get_required_*: required argument was not parsed");

  return arg;
}

KL_StrConst kl_cli_command_get_required_string(KL_CLICommand* cmd, KL_StrConst name) {
  return get_required_arg(cmd, name, KL_CLI_ARG_STRING).as.string;
}

int64_t kl_cli_command_get_required_integer(KL_CLICommand* cmd, KL_StrConst name) {
  return get_required_arg(cmd, name, KL_CLI_ARG_INTEGER).as.integer;
}

bool kl_cli_command_get_required_boolean(KL_CLICommand* cmd, KL_StrConst name) {
  return get_required_arg(cmd, name, KL_CLI_ARG_BOOLEAN).as.boolean;
}

double kl_cli_command_get_required_float(KL_CLICommand* cmd, KL_StrConst name) {
  return get_required_arg(cmd, name, KL_CLI_ARG_FLOAT).as.floating;
}
