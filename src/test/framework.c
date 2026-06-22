#include <kl/test/framework.h>
#include <kl/time/time.h>

// Shared color state — see framework.h.
bool g_use_color = true;
bool g_use_color_initialized = false;

// Shared run-wide counters — see framework.h.
int g_suites_passed = 0;
int g_suites_failed = 0;
int g_tests_passed = 0;
int g_tests_failed = 0;
long long g_run_start_time_us = 0;
KL_TestFunctionContext* g_current_ctx = NULL;

void kl_test_init_color(void) {
  if (g_use_color_initialized) return;
  const char* force_color = getenv("FORCE_COLOR");
  if (force_color && strcmp(force_color, "0") != 0) {
    g_use_color = true;
    g_use_color_initialized = true;
    return;
  }
  const char* no_color = getenv("NO_COLOR");
  if (no_color && strcmp(no_color, "") != 0 && strcmp(no_color, "0") != 0) {
    g_use_color = false;
    g_use_color_initialized = true;
    return;
  }
  g_use_color = isatty(1); // Check if stdout (fd 1) is a terminal
  g_use_color_initialized = true;
}

long long kl_test_get_time_us(void) {
  struct timespec ts;
  if (clock_gettime(CLOCK_MONOTONIC, &ts) == 0) {
    return (long long)ts.tv_sec * 1000000LL + (long long)ts.tv_nsec / 1000LL;
  }
  return 0;
}

void kl_test_print_duration(long long us) {
  if (us < 100) {
    printf("%s(<0.1ms)%s", ANSI_GRAY, ANSI_RESET);
  } else {
    printf("%s(%.1fms)%s", ANSI_GRAY, (double)us / 1000.0, ANSI_RESET);
  }
}

void kl_test_print_summary(void) {
  long long total_time_us = kl_test_get_time_us() - g_run_start_time_us;

  printf("\n");

  // Test Suites line
  printf("%s%sTest Suites%s  ", ANSI_BOLD, ANSI_WHITE, ANSI_RESET);
  if (g_suites_failed > 0) {
    printf("%s%d failed%s, ", ANSI_RED, g_suites_failed, ANSI_RESET);
  }
  printf("%s%d passed%s (%d total)\n", ANSI_GREEN, g_suites_passed, ANSI_RESET, g_suites_passed + g_suites_failed);

  // Tests line
  printf("%s%s     Tests%s  ", ANSI_BOLD, ANSI_WHITE, ANSI_RESET);
  if (g_tests_failed > 0) {
    printf("%s%d failed%s, ", ANSI_RED, g_tests_failed, ANSI_RESET);
  }
  printf("%s%d passed%s (%d total)\n", ANSI_GREEN, g_tests_passed, ANSI_RESET, g_tests_passed + g_tests_failed);

  // Time line
  printf("%s%s      Time%s  ", ANSI_BOLD, ANSI_WHITE, ANSI_RESET);
  if (total_time_us < 100) {
    printf("<0.1ms\n");
  } else if (total_time_us < 1000000) {
    printf("%.1fms\n", (double)total_time_us / 1000.0);
  } else {
    printf("%.2fs\n", (double)total_time_us / 1000000.0);
  }
  printf("\n");
}

void kl_test_run_entry(const KL_TestEntry* entry) {
  KL_TestFunctionContext ctx = {0};
  ctx.current_suite_name = entry->name;
  g_current_ctx = &ctx;
  KL_TEST_BEGIN(entry->name);
  entry->fn(&ctx);
  KL_TEST_END(entry->name);
  g_current_ctx = NULL;
}

void kl_test_run_entry_get_info(const KL_TestEntry* entry, KL_TestInfo* out) {
  KL_TestFunctionContext ctx = {0};
  ctx.current_suite_name = entry->name;
  g_current_ctx = &ctx;
  KL_TEST_BEGIN(entry->name);
  double started_at = kl_time_now_ms__monotonic();
  entry->fn(&ctx);
  double now = kl_time_now_ms__monotonic();
  double duration_ms = now - started_at;
  out->duration_ms = duration_ms;
  KL_TEST_END(entry->name);
  g_current_ctx = NULL;
}
