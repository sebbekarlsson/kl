#include <kl/cli/program.h>
#include <kl/string/string.h>
#include <kl/hash/hash.h>
#include <kl/templates/hashmap/int64_hashmap.h>
#include <kl/tui/table.h>
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

static int cmd_table_fn(KL_CLICommand *cmd) {
  (void) cmd;


  KL_Allocator allocator = {0};
  kl_allocator_init(&allocator, (KL_AllocatorConfig){
    .capacity = 2*1024,
    .track_allocations = true,
    .tracked_allocations_capacity = 16
  });

  KL_TUI_Table table = {0};
  kl_tui_table_init(&table, (KL_TUI_TableConfig){
    .allocator = &allocator,
  });

  kl_tui_table_add_row(&table, (KL_StrConst[]){ KL_STR_CONST_LIT("firstname"), KL_STR_CONST_LIT("lastname"), KL_STR_CONST_LIT("age"), KL_STR_CONST_LIT("score") }, 4);

  kl_tui_table_begin_row(&table);
  kl_tui_table_add_column(&table, "john");
  kl_tui_table_add_column(&table, "doe");
  kl_tui_table_add_column(&table, "%d", 45);
  kl_tui_table_add_column(&table, "%1.6f", 3.5f);
  kl_tui_table_end_row(&table);


  kl_tui_table_begin_row(&table);
  kl_tui_table_add_column(&table, "sarah");
  kl_tui_table_add_column(&table, "sharp");
  kl_tui_table_add_column(&table, "%d", 46);
  kl_tui_table_add_column(&table, "%1.6f", 7.2f);
  kl_tui_table_end_row(&table);

  kl_tui_table_begin_row(&table);
  kl_tui_table_add_column(&table, "boris");
  kl_tui_table_add_column(&table, "kent");
  kl_tui_table_add_column(&table, "%d", 33);
  kl_tui_table_add_column(&table, "%1.6f", 0.583f);
  kl_tui_table_end_row(&table);

  for (int i = 0; i < 1000; i++) {
    kl_tui_table_begin_row(&table);
    kl_tui_table_add_column(&table, "first_%d", i);
    kl_tui_table_add_column(&table, "last_%d", i * 7);
    kl_tui_table_add_column(&table, "%d", i % 100);
    kl_tui_table_add_column(&table, "%1.6f", (float)i * 0.31f);
    kl_tui_table_end_row(&table);
  }

  kl_tui_table_print(&table, stdout);

  kl_allocator_destroy(&allocator);
  
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

  kl_cli_program_define_command(
    &program,
    (KL_CLICommandConfig){
      .name = KL_STR_CONST_LIT("table"),
      .fn = cmd_table_fn,
    }
  );

  int ret = kl_cli_program_run(&program, argc, argv);

  kl_cli_program_destroy(&program);
  return ret;
}
