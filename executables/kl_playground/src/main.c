#include <kl/cli/program.h>
#include <kl/string/string.h>
#include <kl/hash/hash.h>
#include <kl/templates/hashmap/int64_hashmap.h>
#include <assert.h>
#include <math.h>
#include <stdio.h>


static inline void int64_to_string(void* value, KL_StrBuffer* out) {
  kl_str_buffer_append_fmt(out, "%ld", *(int64_t*)value);
}

static int cmd_hash_fn(KL_CLICommand *cmd) {

  KL_int64_Hashmap map = {0};
  kl_int64_hashmap_init(&map, (KL_HashmapConfig){
    .capacity = 5013,
    .collision_capacity = 4,
    .value_to_string = int64_to_string
  });

  kl_int64_hashmap_set(&map, KL_STR_CONST_TO_BLOB(KL_STR_CONST_LIT("x")), 3);
  kl_int64_hashmap_set(&map, KL_STR_CONST_TO_BLOB(KL_STR_CONST_LIT("y")), -4);
  kl_int64_hashmap_set(&map, KL_STR_CONST_TO_BLOB(KL_STR_CONST_LIT("z")), 10);

  kl_int64_hashmap_print_json(&map, stdout);
  
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
