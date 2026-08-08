#ifndef KL_TUI_TABLE_H
#define KL_TUI_TABLE_H

#include <kl/templates/list/str_const_list.h>
#include <kl/templates/list/list.h>
#include <kl/memory/allocator.h>
#include <kl/macros.h>
#include <stdio.h>

#include <stdint.h>
#include <stdbool.h>

typedef struct {
  KL_str_const_List columns;
} KL_TUI_TableRow;

KL_DEFINE_LIST(tui_table_row, KL_TUI_TableRow);

typedef struct {
  KL_Allocator* allocator; // required
  int64_t row_capacity;
  int64_t column_capacity;
} KL_TUI_TableConfig;

typedef struct {
  bool initialized;
  KL_TUI_TableConfig config;
  KL_tui_table_row_List rows;

  struct {
    int64_t index;
  } current_row;
} KL_TUI_Table;

void kl_tui_table_init(KL_TUI_Table* table, KL_TUI_TableConfig cfg);

void kl_tui_table_reset(KL_TUI_Table* table);

void kl_tui_table_destroy(KL_TUI_Table* table);

int64_t kl_tui_table_begin_row(KL_TUI_Table* table);
void kl_tui_table_end_row(KL_TUI_Table* table);

int64_t kl_tui_table_add_row(KL_TUI_Table* table, KL_StrConst* columns, int64_t length);

void kl_tui_table_add_column(KL_TUI_Table* table, const char* fmt, ...)
  __attribute__((format(printf, 2, 3)));


KL_TUI_TableRow* kl_tui_table_get_row(KL_TUI_Table* table, int64_t idx);

void kl_tui_table_print(KL_TUI_Table* table, FILE* fp);

#endif
