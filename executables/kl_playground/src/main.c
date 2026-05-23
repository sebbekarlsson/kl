#include <kl/cli/program.h>
#include <kl/string/string.h>
#include <stdio.h>

static int cmd_foo(KL_CLICommand *cmd) {
  printf("command `foo` received %ld arguments.\n", cmd->parsed_args.length);
  for (int64_t i = 0; i < cmd->parsed_args.length; i++) {
    KL_CLIArgParsed p = cmd->parsed_args.items[i];
    printf("arg %s at %ld is of type %s\n", p.name.value, i,
           p.type == KL_CLI_ARG_UNDEFINED ? "undefined"
           : p.type == KL_CLI_ARG_BOOLEAN ? "boolean"
           : p.type == KL_CLI_ARG_INTEGER ? "integer"
           : p.type == KL_CLI_ARG_FLOAT   ? "float"
           : p.type == KL_CLI_ARG_STRING  ? "string"
                                          : "unknown");


    switch (p.type) {
      case KL_CLI_ARG_UNDEFINED: printf("value: undefined\n"); break;
      case KL_CLI_ARG_BOOLEAN: printf("value: %s\n", p.as.boolean ? "true" : "false"); break;
      case KL_CLI_ARG_STRING: printf("value: %s\n", p.as.string.value); break;
      case KL_CLI_ARG_INTEGER: printf("value: %ld\n", p.as.integer); break;
      case KL_CLI_ARG_FLOAT: printf("value: %g\n", p.as.floating); break;
    }
  }

  return 0;
}

int main(int argc, char *argv[]) {

  KL_CLIProgram program = {0};
  kl_cli_program_init(&program, (KL_CLIProgramConfig){0});
  
  KL_CLICommand* foo = kl_cli_program_define_command(
    &program,
    (KL_CLICommandConfig){
      .name = KL_STR_CONST_LIT("foo"),
      .fn = cmd_foo,
    }
  );
  kl_cli_command_add_option(foo, (KL_CLIArgConfig){
    .name = KL_STR_CONST_LIT("x"),
    .type = KL_CLI_ARG_BOOLEAN
  });

  int ret = kl_cli_program_run(&program, argc, argv);

  kl_cli_program_destroy(&program);
  return ret;
}
