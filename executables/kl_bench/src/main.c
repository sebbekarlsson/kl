#include <kl/memory/allocator.h>
#include <kl/templates/list/list.h>
#include <test/entries.h>
#include <test/tests.h>
#include <kl/cli/args.h>
#include <kl/macros.h>
#include <kl/time/time.h>
#include <assert.h>
#include <time.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>


KL_DEFINE_LIST(test_info, KL_TestInfo);
KL_IMPL_LIST(test_info, KL_TestInfo);

KL_DEFINE_LIST(test_info_list, KL_test_info_List);
KL_IMPL_LIST(test_info_list, KL_test_info_List);


typedef struct {
  double min_ms;
  double median_ms;
  double mean_ms;
} KL_BenchStat;


// Runs every test entry once, recording per-entry timings into a fresh row that
// is appended to `out` (one row per measured iteration).
static void run_tests(KL_test_info_list_List* out, KL_Allocator* allocator) {
  KL_test_info_List infos = {0};
  kl_test_info_list_init(&infos, (KL_ListConfig){ .capacity = TEST_ENTRIES_LEN + 1, .allocator = allocator });

  for (size_t i = 0; i < TEST_ENTRIES_LEN; i++) {
    const KL_TestEntry* entry = &TEST_ENTRIES[i];
    KL_TestInfo info = {0};
    kl_test_run_entry_get_info(entry, &info);
    kl_test_info_list_push(&infos, info);
  }

  kl_test_info_list_list_push(out, infos);
}

// Runs every entry once and discards the timings — primes caches, branch
// predictors and lazy allocations so the first measured iteration isn't skewed.
static void warmup_tests(void) {
  for (size_t i = 0; i < TEST_ENTRIES_LEN; i++) {
    KL_TestInfo info = {0};
    kl_test_run_entry_get_info(&TEST_ENTRIES[i], &info);
  }
}

static int cmp_double(const void* a, const void* b) {
  double da = *(const double*)a;
  double db = *(const double*)b;
  if (da < db) return -1;
  if (da > db) return 1;
  return 0;
}

// Reduces the iterations × entries matrix into one stat row per entry. `samples`
// is a scratch buffer of length `n_iterations`.
static void compute_stats(const KL_test_info_list_List* matrix, int64_t n_iterations,
                          double* samples, KL_BenchStat* out_stats) {
  for (int64_t i = 0; i < (int64_t)TEST_ENTRIES_LEN; i++) {
    double sum = 0.0;
    for (int64_t j = 0; j < n_iterations; j++) {
      assert(j < matrix->length);
      assert(i < matrix->items[j].length);
      double ms = matrix->items[j].items[i].duration_ms;
      samples[j] = ms;
      sum += ms;
    }

    qsort(samples, n_iterations, sizeof(double), cmp_double);

    double median;
    if (n_iterations % 2 == 0) {
      median = (samples[n_iterations / 2 - 1] + samples[n_iterations / 2]) / 2.0;
    } else {
      median = samples[n_iterations / 2];
    }

    out_stats[i] = (KL_BenchStat){
      .min_ms    = samples[0],
      .median_ms = median,
      .mean_ms   = sum / (double)n_iterations,
    };
  }
}

static void write_report(FILE* fp, const KL_BenchStat* stats, int64_t n_iterations, int64_t n_warmup) {
  char timestamp[64] = {0};
  time_t now = time(0);
  struct tm* local = localtime(&now);
  strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M", local);

  fprintf(fp, "# Benchmark report (%s)\n", timestamp);
  fprintf(fp, "----\n");
  fprintf(fp, "iterations=%ld, warmup=%ld\n\n", (long)n_iterations, (long)n_warmup);

  int64_t max_col_w = strlen("name");
  for (int64_t i = 0; i < (int64_t)TEST_ENTRIES_LEN; i++) {
    int64_t len = strlen(TEST_ENTRIES[i].name);
    max_col_w = KL_MAX(max_col_w, len);
  }

  fprintf(fp, "%-*s  %-14s%-14s%s\n", (int)max_col_w, "name", "min_ms", "median_ms", "mean_ms");
  for (int64_t i = 0; i < (int64_t)TEST_ENTRIES_LEN; i++) {
    fprintf(fp, "%-*s  %-14.6f%-14.6f%.6f\n",
            (int)max_col_w, TEST_ENTRIES[i].name,
            stats[i].min_ms, stats[i].median_ms, stats[i].mean_ms);
  }
}


int main(int argc, char* argv[]) {

  KL_Allocator allocator = {0};
  kl_allocator_init(&allocator, (KL_AllocatorConfig){
    .capacity = 4096
  });

  KL_StrConst report_filepath = kl_cli_get_arg(argc, argv, KL_STR_CONST_LIT("--report"), false);
  int64_t n_iterations = kl_cli_get_arg_int(argc, argv, KL_STR_CONST_LIT("--iterations"), 16);
  int64_t n_warmup = kl_cli_get_arg_int(argc, argv, KL_STR_CONST_LIT("--warmup"), 1);

  // Zero-guard: a non-positive iteration count would divide-by-zero in the mean
  // and index past the empty matrix.
  if (n_iterations < 1) n_iterations = 1;
  if (n_warmup < 0) n_warmup = 0;

  for (int64_t i = 0; i < n_warmup; i++) {
    warmup_tests();
  }

  KL_test_info_list_List matrix = {0};
  kl_test_info_list_list_init(&matrix, (KL_ListConfig){
    .capacity = n_iterations + 1,
    .allocator = &allocator
  });

  for (int64_t i = 0; i < n_iterations; i++) {
    run_tests(&matrix, &allocator);
  }

  double* samples = (double*)kl_allocator_malloc(&allocator, (int64_t)sizeof(double) * n_iterations);
  KL_BenchStat* stats = (KL_BenchStat*)kl_allocator_malloc(&allocator, (int64_t)sizeof(KL_BenchStat) * TEST_ENTRIES_LEN);
  compute_stats(&matrix, n_iterations, samples, stats);

  write_report(stdout, stats, n_iterations, n_warmup);

  if (report_filepath.value != 0) {
    FILE* fp = fopen(report_filepath.value, "w+");
    assert(fp != 0);
    write_report(fp, stats, n_iterations, n_warmup);
    fclose(fp);
    fprintf(stdout, "\nWrote to: %s\n", report_filepath.value);
  }

  kl_allocator_destroy(&allocator);

  return 0;
}
