#include <test/tests.h>
#include <test/entries.h>
#include <kl/cli/args.h>
#include <kl/macros.h>
#include <assert.h>
#include <time.h>


static inline bool filter_test(KL_StrConst filter, const char* test_name) {
  return strstr(test_name, filter.value) != 0;
}


int main(int argc, char* argv[]) {
  

  int64_t n_tests = (int64_t)TEST_ENTRIES_LEN;

  KL_StrConst test_to_run = kl_cli_get_arg(argc, argv, KL_STR_CONST_LIT("--test"), false);
  KL_StrConst test_filter = kl_cli_get_arg(argc, argv, KL_STR_CONST_LIT("--filter"), false);
  KL_StrConst report_filepath = kl_cli_get_arg(argc, argv, KL_STR_CONST_LIT("--report"), false);


  if (report_filepath.value != 0) {
    FILE* fp = fopen(report_filepath.value, "w+");
    assert(fp != 0);

    char timestamp[64] = {0};
    time_t now = time(0);
    struct tm *local = localtime(&now);
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M", local);

    fprintf(fp, "# Test report (%s)\n", timestamp);
    fprintf(fp, "----\n");
    

    int64_t max_col_w = 0;
    for (int64_t i = 0; i < n_tests; i++) {
      const KL_TestEntry* entry = &TEST_ENTRIES[i];
      if (test_filter.value != 0 && !filter_test(test_filter, entry->name)) continue;
      
      int64_t len = strlen(entry->name);
      max_col_w = KL_MAX(max_col_w, len);
    }

    fprintf(fp, "name%*sduration_ms\n", KL_MAX(0, (int)max_col_w - 3), "");
    
    for (int64_t i = 0; i < n_tests; i++) {
      const KL_TestEntry* entry = &TEST_ENTRIES[i];
      if (test_filter.value != 0 && !filter_test(test_filter, entry->name)) continue;
      
      int64_t len = strlen(entry->name);
      int pad = KL_MAX(0, (int)max_col_w - (int)len) + 1;
      
      
      KL_TestInfo info = {0};
      kl_test_run_entry_get_info(entry, &info);

      fprintf(fp, "%s%*s%1.13f\n", entry->name, pad, "", info.duration_ms);
      
    }
    fprintf(stdout, "Wrote to: %s\n", report_filepath.value);
    fclose(fp);
    return 0;
  }
  

  if (test_to_run.value != 0) {
    bool found = false;
    for (int64_t i = 0; i < n_tests; i++) {
      const KL_TestEntry* entry = &TEST_ENTRIES[i];
      if (kl_str_const_eq(test_to_run, KL_STR_CONST(entry->name))) {
        kl_test_run_entry(entry);
        found = true;
        break;
      }
    }
    if (!found) {
      fprintf(stderr, "No such test: '%s'\n", test_to_run.value);
      exit(1);
    }
  } else {
    for (int64_t i = 0; i < n_tests; i++) {
      const KL_TestEntry* entry = &TEST_ENTRIES[i];
      kl_test_run_entry(entry);
    }
  }

  kl_test_print_summary();
  return 0;
}
