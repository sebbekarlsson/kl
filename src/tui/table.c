#include <kl/tui/table.h>
#include <kl/macros.h>
#include <kl/string/string.h>
#include <string.h>
#include <stdarg.h>
#include <assert.h>


KL_IMPL_LIST(tui_table_row, KL_TUI_TableRow);

void kl_tui_table_init(KL_TUI_Table* table, KL_TUI_TableConfig cfg) {
  if (table->initialized) return;
  assert(cfg.allocator != 0);
  assert(cfg.allocator->initialized);
  if (cfg.row_capacity <= 0) cfg.row_capacity = 256;
  if (cfg.column_capacity <= 0) cfg.column_capacity = 16;
  table->config = cfg;


  kl_tui_table_row_list_init(&table->rows, (KL_ListConfig){
    .capacity = cfg.row_capacity,
    .allocator = cfg.allocator
  });

  table->current_row.index = -1;
  
  table->initialized = true;
}

void kl_tui_table_reset(KL_TUI_Table* table) {
  assert(table->initialized);

  for (int64_t i = 0; i < table->rows.length; i++) {
    KL_TUI_TableRow* row = &table->rows.items[i];
    kl_str_const_list_destroy(&row->columns);
  }

  kl_tui_table_row_list_reset_fast(&table->rows);

  table->current_row.index = -1;
}

void kl_tui_table_destroy(KL_TUI_Table* table) {
  assert(table->initialized);

  for (int64_t i = 0; i < table->rows.length; i++) {
    KL_TUI_TableRow* row = &table->rows.items[i];
    kl_str_const_list_destroy(&row->columns);
  }

  kl_tui_table_row_list_destroy(&table->rows);

  memset(table, 0, sizeof(KL_TUI_Table));

  table->current_row.index = -1;
}

int64_t kl_tui_table_begin_row(KL_TUI_Table* table) {
  assert(table->initialized);
  assert(table->current_row.index < 0);

  int64_t idx = table->rows.length;
  KL_TUI_TableRow row = {0};
  kl_str_const_list_init(&row.columns, (KL_ListConfig){
    .allocator = table->config.allocator,
    .capacity = table->config.column_capacity
  });

  kl_tui_table_row_list_push(&table->rows, row);

  table->current_row.index = idx;

  return idx;
}

void kl_tui_table_end_row(KL_TUI_Table* table) {
  assert(table->initialized);
  assert(table->current_row.index >= 0);
  table->current_row.index = -1;
}

int64_t kl_tui_table_add_row(KL_TUI_Table* table, KL_StrConst* columns, int64_t length) {
  assert(table->initialized);
  assert(columns != 0);
  assert(length > 0);

  int64_t idx = kl_tui_table_begin_row(table);

  KL_TUI_TableRow* row = kl_tui_table_get_row(table, idx);
  assert(row != 0);
  assert(row->columns.initialized);

  for (int64_t i = 0; i < length; i++) {
    KL_StrConst col = columns[i];
    assert(col.value != 0);
    assert(col.length > 0);
    kl_str_const_list_push(&row->columns, kl_str_const_strdup(col, table->config.allocator));
  }

  kl_tui_table_end_row(table);

  return idx;
}

__attribute__((format(printf, 2, 3)))
void kl_tui_table_add_column  (KL_TUI_Table* table, const char* fmt, ...) {
  if (fmt == 0) return;
  
  assert(table->initialized);

  int64_t idx = table->current_row.index;
  assert(idx >= 0);

  KL_TUI_TableRow* row = kl_tui_table_get_row(table, idx);
  assert(row != 0);
  assert(row->columns.initialized);


  va_list args;
  va_start(args, fmt);

  va_list measure;
  va_copy(measure, args);
  int needed = vsnprintf(0, 0, fmt, measure);
  va_end(measure);

  if (needed < 0) {
    va_end(args);
    return;
  }

  if (needed == 0) {
    va_end(args);
  }

  KL_StrBuffer buff = {0};
  buff.allocator = table->config.allocator;
  buff.bump_size = 64;
  buff.length = 0;

  kl_str_buffer_fit(&buff, needed + 1);
  int64_t avail = kl_str_buffer_avail(&buff);
  assert(avail >= needed);

  int written = vsnprintf(&buff.data[buff.length], needed + 1, fmt, args);
  va_end(args);

  if (written < 0) {
    kl_allocator_free_allocation(table->config.allocator, buff.allocation);
    return;
  }

  buff.length += written;

  kl_str_const_list_push(&row->columns, kl_str_const_strdup(KL_STR_BUFFER_TO_STR_CONST(buff), table->config.allocator));

  kl_allocator_free_allocation(table->config.allocator, buff.allocation);
}

KL_TUI_TableRow* kl_tui_table_get_row(KL_TUI_Table* table, int64_t idx) {
  assert(table->initialized);
  if (table->rows.length <= 0 || table->rows.items == 0) return 0;
  assert(idx >= 0);
  assert(idx < table->rows.length);
  return &table->rows.items[idx];
}

static void tui_table_print_border(FILE* fp, const int64_t* widths, int64_t n_cols,
                                   const char* left, const char* mid, const char* right) {
  fputs(left, fp);
  for (int64_t j = 0; j < n_cols; j++) {
    for (int64_t k = 0; k < widths[j] + 2; k++) fputs("─", fp);
    fputs(j < n_cols - 1 ? mid : right, fp);
  }
  fputc('\n', fp);
}

void kl_tui_table_print(KL_TUI_Table* table, FILE* fp) {
  assert(table->initialized);
  assert(fp != 0);
  if (table->rows.length <= 0 || table->rows.items == 0) return;

  // measure column count and per-column widths
  int64_t n_cols = 0;
  for (int64_t i = 0; i < table->rows.length; i++) {
    n_cols = KL_MAX(n_cols, table->rows.items[i].columns.length);
  }
  if (n_cols <= 0) return;

  KL_Allocation widths_allocation = {0};
  int64_t* widths = (int64_t*)kl_allocator_malloc_tracked(
    table->config.allocator, n_cols * (int64_t)sizeof(int64_t), &widths_allocation);
  assert(widths != 0);
  memset(widths, 0, n_cols * sizeof(int64_t));

  for (int64_t i = 0; i < table->rows.length; i++) {
    KL_TUI_TableRow row = table->rows.items[i];
    for (int64_t j = 0; j < row.columns.length; j++) {
      widths[j] = KL_MAX(widths[j], row.columns.items[j].length);
    }
  }

  tui_table_print_border(fp, widths, n_cols, "┌", "┬", "┐");

  for (int64_t i = 0; i < table->rows.length; i++) {
    KL_TUI_TableRow row = table->rows.items[i];
    fputs("│", fp);
    for (int64_t j = 0; j < n_cols; j++) {
      KL_StrConst col = j < row.columns.length ? row.columns.items[j]
                                               : (KL_StrConst){ .value = "", .length = 0 };
      fprintf(fp, " %-*s │", (int)widths[j], col.value);
    }
    fputc('\n', fp);

    // separator between the header (first row) and the body
    if (i == 0 && table->rows.length > 1) {
      tui_table_print_border(fp, widths, n_cols, "├", "┼", "┤");
    }
  }

  tui_table_print_border(fp, widths, n_cols, "└", "┴", "┘");

  kl_allocator_free_allocation(table->config.allocator, widths_allocation);
}
