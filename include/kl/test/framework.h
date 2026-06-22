#ifndef KL_TEST_FRAMEWORK_H
#define KL_TEST_FRAMEWORK_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <stdbool.h>
#include <math.h>
#include <kl/string/string.h>

#define KL_TEST_TEMP_BUFFER_CAP 2048
#define KL_TEST_MAX_SUBTESTS 128

// Dynamic color support. The state lives in framework.c so every test
// translation unit shares a single instance.
extern bool g_use_color;
extern bool g_use_color_initialized;

void kl_test_init_color(void);

// ANSI Escape sequences
#define ANSI_RED      (g_use_color ? "\033[31m" : "")
#define ANSI_GREEN    (g_use_color ? "\033[32m" : "")
#define ANSI_YELLOW   (g_use_color ? "\033[33m" : "")
#define ANSI_BLUE     (g_use_color ? "\033[34m" : "")
#define ANSI_MAGENTA  (g_use_color ? "\033[35m" : "")
#define ANSI_CYAN     (g_use_color ? "\033[36m" : "")
#define ANSI_WHITE    (g_use_color ? "\033[37m" : "")
#define ANSI_GRAY     (g_use_color ? "\033[90m" : "")
#define ANSI_BOLD     (g_use_color ? "\033[1m" : "")
#define ANSI_DIM      (g_use_color ? "\033[2m" : "")
#define ANSI_RESET    (g_use_color ? "\033[0m" : "")

#define ANSI_BG_RED   (g_use_color ? "\033[41m" : "")
#define ANSI_BG_GREEN (g_use_color ? "\033[42m" : "")

// Subtest result entry
typedef struct {
  char name[256];
  long long duration_us;
  bool passed;
} KL_TestSubTest;

// Test execution context
typedef struct {
  char tmp[KL_TEST_TEMP_BUFFER_CAP];
  long long start_time_us;
  long long it_start_time_us;
  KL_TestSubTest subtests[KL_TEST_MAX_SUBTESTS];
  int subtests_count;
  const char* current_suite_name;
  bool suite_failed;
} KL_TestFunctionContext;

typedef void (*KL_TestFunction)(KL_TestFunctionContext* ctx);

typedef struct {
  KL_TestFunction fn;
  const char* name;
} KL_TestEntry;

// Global stats counters. Defined in framework.c so the totals aggregate
// across every test translation unit.
extern int g_suites_passed;
extern int g_suites_failed;
extern int g_tests_passed;
extern int g_tests_failed;
extern long long g_run_start_time_us;
extern KL_TestFunctionContext* g_current_ctx;

// Monotonic clock timing helper (microseconds)
long long kl_test_get_time_us(void);

// Visual duration printer
void kl_test_print_duration(long long us);

// Summary stats printer
void kl_test_print_summary(void);

// Run a single test entry (sets up context, calls BEGIN/END)
void kl_test_run_entry(const KL_TestEntry* entry);

typedef struct {
  double duration_ms;
} KL_TestInfo;

void kl_test_run_entry_get_info(const KL_TestEntry* entry, KL_TestInfo* out);

#define KL_TEST_STR(A) #A

#define KL_TEST_ENTRY(FN) ((KL_TestEntry){ .fn = (FN), .name = KL_TEST_STR(FN) })

// Macro triggers at suite start
#define KL_TEST_BEGIN(NAME) do { \
  kl_test_init_color(); \
  if (g_run_start_time_us == 0) { \
    g_run_start_time_us = kl_test_get_time_us(); \
  } \
  if (g_current_ctx) { \
    g_current_ctx->start_time_us = kl_test_get_time_us(); \
    g_current_ctx->subtests_count = 0; \
    g_current_ctx->current_suite_name = NAME; \
    g_current_ctx->suite_failed = false; \
  } \
} while (0)

// Macro triggers at suite completion
#define KL_TEST_END(NAME) do { \
  if (g_current_ctx) { \
    long long duration_us = kl_test_get_time_us() - g_current_ctx->start_time_us; \
    if (g_current_ctx->suite_failed) { \
      g_suites_failed++; \
    } else { \
      g_suites_passed++; \
      if (g_current_ctx->subtests_count == 0) { \
        g_tests_passed++; \
      } \
      printf("  %s✓%s %s%s%s ", ANSI_GREEN, ANSI_RESET, ANSI_BOLD, NAME, ANSI_RESET); \
      kl_test_print_duration(duration_us); \
      printf("\n"); \
    } \
    for (int i = 0; i < g_current_ctx->subtests_count; i++) { \
      KL_TestSubTest* sub = &g_current_ctx->subtests[i]; \
      if (sub->passed) { \
        printf("    %s✓%s %s%s%s ", ANSI_GREEN, ANSI_RESET, ANSI_DIM, sub->name, ANSI_RESET); \
        kl_test_print_duration(sub->duration_us); \
        printf("\n"); \
      } else { \
        printf("    %s×%s %s%s%s ", ANSI_RED, ANSI_RESET, ANSI_RED, sub->name, ANSI_RESET); \
        kl_test_print_duration(sub->duration_us); \
        printf("\n"); \
      } \
    } \
  } \
} while (0)

// Sub-test block definition
#define KL_TEST_IT(CTX, NAME) do { \
  memset((CTX)->tmp, 0, KL_TEST_TEMP_BUFFER_CAP); \
  snprintf((CTX)->tmp, KL_TEST_TEMP_BUFFER_CAP-1, "%s", NAME); \
  (CTX)->it_start_time_us = kl_test_get_time_us(); \
} while (0)

// Sub-test block end
#define KL_TEST_IT_END(CTX) do { \
  long long it_duration_us = kl_test_get_time_us() - (CTX)->it_start_time_us; \
  if ((CTX)->subtests_count < KL_TEST_MAX_SUBTESTS) { \
    KL_TestSubTest* sub = &(CTX)->subtests[(CTX)->subtests_count++]; \
    int _i = 0; \
    while (_i < (int)sizeof(sub->name) - 1 && (CTX)->tmp[_i] != '\0') { \
      sub->name[_i] = (CTX)->tmp[_i]; \
      _i++; \
    } \
    sub->name[_i] = '\0'; \
    sub->duration_us = it_duration_us; \
    sub->passed = true; \
    g_tests_passed++; \
  } \
  memset((CTX)->tmp, 0, KL_TEST_TEMP_BUFFER_CAP); \
} while (0)

// High-fidelity error reporter
#define KL_TEST_PRINT_ASSERT_FAILURE(MSG) do { \
  g_tests_failed++; \
  if (g_current_ctx) { \
    g_current_ctx->suite_failed = true; \
    g_suites_failed++; \
    long long duration_us = kl_test_get_time_us() - g_current_ctx->start_time_us; \
    printf(" %s%s FAIL %s %s%s ", ANSI_BG_RED, ANSI_WHITE, ANSI_RESET, ANSI_BOLD, g_current_ctx->current_suite_name); \
    kl_test_print_duration(duration_us); \
    printf("\n"); \
    for (int i = 0; i < g_current_ctx->subtests_count; i++) { \
      KL_TestSubTest* sub = &g_current_ctx->subtests[i]; \
      if (sub->passed) { \
        printf("    %s✓%s %s%s%s ", ANSI_GREEN, ANSI_RESET, ANSI_DIM, sub->name, ANSI_RESET); \
        kl_test_print_duration(sub->duration_us); \
        printf("\n"); \
      } \
    } \
    if (g_current_ctx->tmp[0] != '\0') { \
      long long sub_duration_us = kl_test_get_time_us() - g_current_ctx->it_start_time_us; \
      printf("    %s×%s %s%s%s ", ANSI_RED, ANSI_RESET, ANSI_RED, g_current_ctx->tmp, ANSI_RESET); \
      kl_test_print_duration(sub_duration_us); \
      printf("\n"); \
    } \
  } \
  fprintf(stderr, "\n  %sAssertionError: %s%s\n", ANSI_RED, MSG, ANSI_RESET); \
  fprintf(stderr, "  %sat [%s:%d]%s\n\n", ANSI_GRAY, __FILE_NAME__, __LINE__, ANSI_RESET); \
} while (0)

#define KL_TEST_FAILED() do { \
  kl_test_print_summary(); \
  exit(1); \
} while (0)

#define KL_TEST_ASSERT(EXPR, MSG) do { if (!(EXPR)) { KL_TEST_PRINT_ASSERT_FAILURE(MSG); KL_TEST_FAILED(); } } while (0)

#define KL_TEST_EXPECT_EQ_ERROR_MESSAGE(A, B) "expected " KL_TEST_STR(A) " to be equal " KL_TEST_STR(B)
#define KL_TEST_EXPECT_EQ_FLOAT_ERROR_MESSAGE(A, B, T) "expected " KL_TEST_STR(A) " to be within " KL_TEST_STR(T) " of " KL_TEST_STR(B)
#define KL_TEST_EXPECT_EXPR(EXPR) KL_TEST_ASSERT(EXPR, "expected " KL_TEST_STR(EXPR) " to be truthy")
#define KL_TEST_EXPECT_EQ_INT(A, B) KL_TEST_ASSERT((A) == (B), KL_TEST_EXPECT_EQ_ERROR_MESSAGE(A, B))
#define KL_TEST_EXPECT_EQ_CHAR(A, B) KL_TEST_ASSERT((A) == (B), KL_TEST_EXPECT_EQ_ERROR_MESSAGE(A, B))
#define KL_TEST_EXPECT_EQ_FLOAT(A, B, T) KL_TEST_ASSERT(fabsf((float)(A) - (float)(B)) <= (float)(T), KL_TEST_EXPECT_EQ_FLOAT_ERROR_MESSAGE(A, B, T))

// Compares two KL_StrConst for equality, reporting both the actual and the
// expected value on failure. The message is built at runtime so the offending
// string contents are visible in the failure output.
#define KL_TEST_EXPECT_EQ_STR(A, B) do { \
  KL_StrConst _kl_actual = (A); \
  KL_StrConst _kl_expected = (B); \
  if (!kl_str_const_eq(_kl_actual, _kl_expected)) { \
    char _kl_str_msg[KL_TEST_TEMP_BUFFER_CAP]; \
    snprintf(_kl_str_msg, sizeof(_kl_str_msg), \
      "expected \"%.*s\" (len %d) to equal \"%.*s\" (len %d)", \
      (int)_kl_actual.length, _kl_actual.value ? _kl_actual.value : "", (int)_kl_actual.length, \
      (int)_kl_expected.length, _kl_expected.value ? _kl_expected.value : "", (int)_kl_expected.length); \
    KL_TEST_PRINT_ASSERT_FAILURE(_kl_str_msg); \
    KL_TEST_FAILED(); \
  } \
} while (0)

#endif
