#include <kl/cli/program.h>
#include <kl/string/string.h>
#include <kl/hash/hash.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>

static int cmd_hash_fn(KL_CLICommand *cmd) {

  uint32_t seed = 8381U;

  for (int32_t i = 0; i < 1000000; i++) {
    int32_t v = kl_hash_uint32_next_int32(&seed, -10, 10);

    assert(v >= -10);
    assert(v <= 10);
    printf("%d\n", v);
    
    
  }
  
  return 0;
}

int main(int argc, char *argv[]) {

  KL_CLIProgram program = {0};
  kl_cli_program_init(&program, (KL_CLIProgramConfig){0});
  
  KL_CLICommand* cmd_hash = kl_cli_program_define_command(
    &program,
    (KL_CLICommandConfig){
      .name = KL_STR_CONST_LIT("hash"),
      .fn = cmd_hash_fn,
    }
  );
  (void)cmd_hash;
  //kl_cli_command_add_option(cmd_hash, (KL_CLIArgConfig){
  //  .name = KL_STR_CONST_LIT("x"),
  //  .type = KL_CLI_ARG_BOOLEAN
  //});

  int ret = kl_cli_program_run(&program, argc, argv);

  kl_cli_program_destroy(&program);
  return ret;
}
