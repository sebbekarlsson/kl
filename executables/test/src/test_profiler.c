#include <test/tests.h>
#include <kl/debug/profiler.h>
#include <kl/time/time.h>
#include <pthread.h>
#include <sched.h>

// ---------- profiler ----------

#define KL_TEST_PROF_CSTR(S) ((KL_StrConst){ .value = (S), .length = (int64_t)strlen(S) })

// Every test writes to its own file under /tmp so tests never share output.
typedef struct {
  char path[256];
  KL_Profiler prof;
} KL_TestProf;

static KL_StrConst kl_test_prof_path(KL_TestProf* fx, const char* name) {
  snprintf(fx->path, sizeof(fx->path), "/tmp/kl_test_profiler_%d_%s.jsonl", (int)getpid(), name);
  return (KL_StrConst){ .value = fx->path, .length = (int64_t)strlen(fx->path) };
}

static bool kl_test_prof_setup(KL_TestProf* fx, const char* name, KL_ProfilerConfig cfg) {
  fx->prof = (KL_Profiler){0};
  cfg.filepath = kl_test_prof_path(fx, name);
  return kl_profiler_init(&fx->prof, cfg);
}

static void kl_test_prof_teardown(KL_TestProf* fx) {
  if (fx->prof.initialized) kl_profiler_destroy(&fx->prof);
  remove(fx->path);
}

// A parsed output line.
typedef struct {
  char label[KL_PROFILER_LABEL_CAP];
  double timestamp;
  double duration;
  int depth;
  unsigned long long thread;
} KL_TestProfLine;

// Parses `{ "label": "...", "timestamp": T, "duration": D, "depth": N, "thread": X }`.
// The label is copied raw (escapes are left as written).
static bool kl_test_prof_parse_line(const char* line, KL_TestProfLine* out) {
  memset(out, 0, sizeof(*out));

  const char* label_key = "\"label\": \"";
  const char* p = strstr(line, label_key);
  if (p == 0) return false;
  p += strlen(label_key);

  // The label ends at the first unescaped quote.
  size_t n = 0;
  while (*p != 0 && n < sizeof(out->label) - 1) {
    if (*p == '\\' && p[1] != 0) {
      out->label[n++] = *p++;
      out->label[n++] = *p++;
      continue;
    }
    if (*p == '"') break;
    out->label[n++] = *p++;
  }
  out->label[n] = 0;

  const char* ts = strstr(p, "\"timestamp\": ");
  const char* du = strstr(p, "\"duration\": ");
  const char* de = strstr(p, "\"depth\": ");
  const char* th = strstr(p, "\"thread\": ");
  if (ts == 0 || du == 0 || de == 0 || th == 0) return false;

  out->timestamp = strtod(ts + strlen("\"timestamp\": "), 0);
  out->duration = strtod(du + strlen("\"duration\": "), 0);
  out->depth = atoi(de + strlen("\"depth\": "));
  out->thread = strtoull(th + strlen("\"thread\": "), 0, 10);
  return true;
}

// Reads at most `cap` lines into `out`; returns the number of lines in the file.
static int64_t kl_test_prof_read(const char* path, KL_TestProfLine* out, int64_t cap) {
  FILE* fp = fopen(path, "r");
  if (fp == 0) return -1;

  char line[1024];
  int64_t count = 0;
  while (fgets(line, sizeof(line), fp) != 0) {
    if (out != 0 && count < cap) {
      if (!kl_test_prof_parse_line(line, &out[count])) {
        fclose(fp);
        return -1;
      }
    }
    count++;
  }

  fclose(fp);
  return count;
}

static int64_t kl_test_prof_line_count(const char* path) {
  return kl_test_prof_read(path, 0, 0);
}

static int64_t kl_test_prof_count_label(KL_TestProfLine* lines, int64_t count, const char* label) {
  int64_t n = 0;
  for (int64_t i = 0; i < count; i++) {
    if (strcmp(lines[i].label, label) == 0) n++;
  }
  return n;
}

// ---------- lifecycle ----------

void test_profiler_init_destroy() {
  KL_TestProf fx = {0};
  KL_TEST_EXPECT_EXPR(kl_test_prof_setup(&fx, "init_destroy", (KL_ProfilerConfig){0}));

  KL_TEST_ASSERT(fx.prof.initialized, "initialized after init");
  KL_TEST_ASSERT(fx.prof.output.fp != 0, "output file opened by init");
  KL_TEST_ASSERT(!atomic_load(&fx.prof.thread.running), "writer thread not started by init");
  KL_TEST_ASSERT(fx.prof.messages.pending.initialized, "pending list initialized");
  KL_TEST_ASSERT(fx.prof.messages.draining.initialized, "draining list initialized");
  KL_TEST_ASSERT(fx.prof.stacks.pool.initialized, "stack pool initialized");

  // defaults
  KL_TEST_EXPECT_EQ_INT((int)fx.prof.config.max_queue_size, 4096);
  KL_TEST_EXPECT_EQ_INT((int)fx.prof.config.max_block_depth, 64);
  KL_TEST_EXPECT_EQ_INT((int)fx.prof.config.max_threads, 16);
  KL_TEST_EXPECT_EQ_INT((int)fx.prof.config.poll_interval_us, 1000);

  // the file exists (and is empty) right after init
  KL_TEST_EXPECT_EQ_INT((int)kl_test_prof_line_count(fx.path), 0);

  kl_profiler_destroy(&fx.prof);
  KL_TEST_ASSERT(!fx.prof.initialized, "initialized cleared after destroy");
  KL_TEST_ASSERT(fx.prof.output.fp == 0, "fp cleared after destroy");
  KL_TEST_EXPECT_EQ_INT((int)kl_test_prof_line_count(fx.path), 0);

  kl_test_prof_teardown(&fx);
}

void test_profiler_init_keeps_explicit_config() {
  KL_TestProf fx = {0};
  KL_TEST_EXPECT_EXPR(kl_test_prof_setup(&fx, "explicit_config", (KL_ProfilerConfig){
    .max_queue_size = 7,
    .max_block_depth = 3,
    .max_threads = 2,
    .poll_interval_us = 50,
  }));

  KL_TEST_EXPECT_EQ_INT((int)fx.prof.config.max_queue_size, 7);
  KL_TEST_EXPECT_EQ_INT((int)fx.prof.config.max_block_depth, 3);
  KL_TEST_EXPECT_EQ_INT((int)fx.prof.config.max_threads, 2);
  KL_TEST_EXPECT_EQ_INT((int)fx.prof.config.poll_interval_us, 50);

  kl_test_prof_teardown(&fx);
}

void test_profiler_init_unwritable_path_fails() {
  KL_Profiler prof = {0};
  bool ok = kl_profiler_init(&prof, (KL_ProfilerConfig){
    .filepath = KL_STR_CONST_LIT("/nonexistent_kl_dir/profile.jsonl"),
  });
  KL_TEST_ASSERT(!ok, "init fails when the file cannot be opened");
  KL_TEST_ASSERT(!prof.initialized, "profiler stays uninitialized");
}

void test_profiler_double_init_is_noop() {
  KL_TestProf fx = {0};
  KL_TEST_EXPECT_EXPR(kl_test_prof_setup(&fx, "double_init", (KL_ProfilerConfig){ .max_queue_size = 5 }));
  FILE* fp = fx.prof.output.fp;

  KL_TEST_EXPECT_EXPR(kl_profiler_init(&fx.prof, (KL_ProfilerConfig){
    .max_queue_size = 999,
    .filepath = KL_STR_CONST_LIT("/nonexistent_kl_dir/other.jsonl"),
  }));
  KL_TEST_EXPECT_EQ_INT((int)fx.prof.config.max_queue_size, 5);
  KL_TEST_ASSERT(fx.prof.output.fp == fp, "second init keeps the original file");

  kl_test_prof_teardown(&fx);
}

void test_profiler_filepath_need_not_be_nul_terminated() {
  // Slice a longer string: the profiler must honour `length`, not scan for NUL.
  KL_TestProf fx = {0};
  KL_StrConst full = kl_test_prof_path(&fx, "sliced.jsonl.IGNORED");
  KL_StrConst sliced = { .value = full.value, .length = full.length - (int64_t)strlen(".IGNORED") };

  fx.prof = (KL_Profiler){0};
  KL_TEST_EXPECT_EXPR(kl_profiler_init(&fx.prof, (KL_ProfilerConfig){ .filepath = sliced }));

  char expected[256];
  snprintf(expected, sizeof(expected), "%.*s", (int)sliced.length, sliced.value);
  KL_TEST_EXPECT_EQ_INT((int)kl_test_prof_line_count(expected), 0);
  KL_TEST_EXPECT_EQ_INT((int)kl_test_prof_line_count(fx.path), -1); // the full name was not created

  kl_profiler_destroy(&fx.prof);
  remove(expected);
}

void test_profiler_start_kill_are_idempotent() {
  KL_TestProf fx = {0};
  KL_TEST_EXPECT_EXPR(kl_test_prof_setup(&fx, "start_kill", (KL_ProfilerConfig){0}));

  kl_profiler_kill(&fx.prof); // not running: no-op
  KL_TEST_ASSERT(!atomic_load(&fx.prof.thread.running), "kill before start is a no-op");

  KL_TEST_EXPECT_EXPR(kl_profiler_start(&fx.prof));
  KL_TEST_ASSERT(atomic_load(&fx.prof.thread.running), "running after start");
  KL_TEST_EXPECT_EXPR(kl_profiler_start(&fx.prof)); // already running
  KL_TEST_ASSERT(atomic_load(&fx.prof.thread.running), "still running after second start");

  kl_profiler_kill(&fx.prof);
  KL_TEST_ASSERT(!atomic_load(&fx.prof.thread.running), "stopped after kill");
  kl_profiler_kill(&fx.prof);
  KL_TEST_ASSERT(!atomic_load(&fx.prof.thread.running), "second kill is a no-op");

  kl_test_prof_teardown(&fx);
}

// ---------- single-thread blocks ----------

void test_profiler_end_without_begin_is_noop() {
  KL_TestProf fx = {0};
  KL_TEST_EXPECT_EXPR(kl_test_prof_setup(&fx, "end_without_begin", (KL_ProfilerConfig){0}));

  KL_TEST_ASSERT(!kl_profiler_end_block(&fx.prof), "end without begin returns false");
  KL_TEST_EXPECT_EQ_INT((int)kl_profiler_depth(&fx.prof), 0);
  KL_TEST_EXPECT_EQ_INT((int)atomic_load(&fx.prof.stats.submitted), 0);
  KL_TEST_EXPECT_EQ_INT((int)atomic_load(&fx.prof.stats.dropped), 0);
  // a thread that never began a block gets no stack
  KL_TEST_EXPECT_EQ_INT((int)fx.prof.stacks.pool.taken_indices.length, 0);

  kl_test_prof_teardown(&fx);
}

void test_profiler_begin_end_submits_point() {
  KL_TestProf fx = {0};
  KL_TEST_EXPECT_EXPR(kl_test_prof_setup(&fx, "begin_end", (KL_ProfilerConfig){0}));

  double before = kl_time_now_ms__monotonic();
  KL_TEST_EXPECT_EXPR(kl_profiler_begin_block(&fx.prof, KL_STR_CONST_LIT("work")));
  KL_TEST_EXPECT_EQ_INT((int)kl_profiler_depth(&fx.prof), 1);
  usleep(2000);
  KL_TEST_EXPECT_EXPR(kl_profiler_end_block(&fx.prof));
  double after = kl_time_now_ms__monotonic();

  KL_TEST_EXPECT_EQ_INT((int)kl_profiler_depth(&fx.prof), 0);
  KL_TEST_EXPECT_EQ_INT((int)atomic_load(&fx.prof.stats.submitted), 1);
  KL_TEST_EXPECT_EQ_INT((int)atomic_load(&fx.prof.stats.written), 0); // no writer yet
  KL_TEST_EXPECT_EQ_INT((int)fx.prof.messages.pending.length, 1);

  // Not started: nothing on disk until destroy drains synchronously.
  KL_TEST_EXPECT_EQ_INT((int)kl_test_prof_line_count(fx.path), 0);
  kl_profiler_destroy(&fx.prof);

  KL_TestProfLine lines[4];
  KL_TEST_EXPECT_EQ_INT((int)kl_test_prof_read(fx.path, lines, 4), 1);
  KL_TEST_EXPECT_EQ_STR(KL_TEST_PROF_CSTR(lines[0].label), KL_STR_CONST_LIT("work"));
  KL_TEST_EXPECT_EQ_INT(lines[0].depth, 0);
  KL_TEST_EXPECT_EXPR(lines[0].duration >= 2.0);
  KL_TEST_EXPECT_EXPR(lines[0].duration <= after - before);
  KL_TEST_EXPECT_EXPR(lines[0].timestamp >= before);
  KL_TEST_EXPECT_EXPR(lines[0].timestamp <= after);
  KL_TEST_EXPECT_EXPR(lines[0].thread == (unsigned long long)pthread_self());

  kl_test_prof_teardown(&fx);
}

void test_profiler_nested_blocks_record_depth_and_lifo_order() {
  KL_TestProf fx = {0};
  KL_TEST_EXPECT_EXPR(kl_test_prof_setup(&fx, "nested", (KL_ProfilerConfig){0}));

  KL_TEST_EXPECT_EXPR(kl_profiler_begin_block(&fx.prof, KL_STR_CONST_LIT("outer")));
  usleep(1000);
  KL_TEST_EXPECT_EXPR(kl_profiler_begin_block(&fx.prof, KL_STR_CONST_LIT("middle")));
  usleep(1000);
  KL_TEST_EXPECT_EXPR(kl_profiler_begin_block(&fx.prof, KL_STR_CONST_LIT("inner")));
  KL_TEST_EXPECT_EQ_INT((int)kl_profiler_depth(&fx.prof), 3);
  usleep(1000);
  KL_TEST_EXPECT_EXPR(kl_profiler_end_block(&fx.prof)); // inner
  KL_TEST_EXPECT_EQ_INT((int)kl_profiler_depth(&fx.prof), 2);
  KL_TEST_EXPECT_EXPR(kl_profiler_end_block(&fx.prof)); // middle
  KL_TEST_EXPECT_EXPR(kl_profiler_end_block(&fx.prof)); // outer
  KL_TEST_EXPECT_EQ_INT((int)kl_profiler_depth(&fx.prof), 0);
  KL_TEST_ASSERT(!kl_profiler_end_block(&fx.prof), "extra end is rejected");

  kl_profiler_destroy(&fx.prof);

  KL_TestProfLine lines[4];
  KL_TEST_EXPECT_EQ_INT((int)kl_test_prof_read(fx.path, lines, 4), 3);

  // Points are emitted when a block *ends*, so innermost comes first.
  KL_TEST_EXPECT_EQ_STR(KL_TEST_PROF_CSTR(lines[0].label), KL_STR_CONST_LIT("inner"));
  KL_TEST_EXPECT_EQ_STR(KL_TEST_PROF_CSTR(lines[1].label), KL_STR_CONST_LIT("middle"));
  KL_TEST_EXPECT_EQ_STR(KL_TEST_PROF_CSTR(lines[2].label), KL_STR_CONST_LIT("outer"));
  KL_TEST_EXPECT_EQ_INT(lines[0].depth, 2);
  KL_TEST_EXPECT_EQ_INT(lines[1].depth, 1);
  KL_TEST_EXPECT_EQ_INT(lines[2].depth, 0);

  // Outer blocks start earlier and last longer than the blocks they contain.
  KL_TEST_EXPECT_EXPR(lines[2].timestamp <= lines[1].timestamp);
  KL_TEST_EXPECT_EXPR(lines[1].timestamp <= lines[0].timestamp);
  KL_TEST_EXPECT_EXPR(lines[2].duration >= lines[1].duration);
  KL_TEST_EXPECT_EXPR(lines[1].duration >= lines[0].duration);
  KL_TEST_EXPECT_EXPR(lines[0].duration >= 1.0);
  KL_TEST_EXPECT_EXPR(lines[1].duration >= 2.0);
  KL_TEST_EXPECT_EXPR(lines[2].duration >= 3.0);

  kl_test_prof_teardown(&fx);
}

void test_profiler_sequential_blocks_keep_submission_order() {
  KL_TestProf fx = {0};
  KL_TEST_EXPECT_EXPR(kl_test_prof_setup(&fx, "order", (KL_ProfilerConfig){0}));

  enum { COUNT = 50 };
  char names[COUNT][16];
  for (int i = 0; i < COUNT; i++) {
    snprintf(names[i], sizeof(names[i]), "block_%02d", i);
    KL_TEST_EXPECT_EXPR(kl_profiler_begin_block(&fx.prof, (KL_StrConst){ .value = names[i], .length = (int64_t)strlen(names[i]) }));
    KL_TEST_EXPECT_EXPR(kl_profiler_end_block(&fx.prof));
  }
  kl_profiler_destroy(&fx.prof);

  KL_TestProfLine lines[COUNT];
  KL_TEST_EXPECT_EQ_INT((int)kl_test_prof_read(fx.path, lines, COUNT), COUNT);
  for (int i = 0; i < COUNT; i++) {
    KL_TEST_EXPECT_EQ_STR(KL_TEST_PROF_CSTR(lines[i].label), KL_TEST_PROF_CSTR(names[i]));
    KL_TEST_EXPECT_EQ_INT(lines[i].depth, 0);
    if (i > 0) KL_TEST_EXPECT_EXPR(lines[i].timestamp >= lines[i - 1].timestamp);
  }

  kl_test_prof_teardown(&fx);
}

void test_profiler_label_is_copied_at_end() {
  KL_TestProf fx = {0};
  KL_TEST_EXPECT_EXPR(kl_test_prof_setup(&fx, "label_copy", (KL_ProfilerConfig){0}));

  char buffer[32];
  snprintf(buffer, sizeof(buffer), "dynamic_%d", 42);
  KL_TEST_EXPECT_EXPR(kl_profiler_begin_block(&fx.prof, (KL_StrConst){ .value = buffer, .length = (int64_t)strlen(buffer) }));
  KL_TEST_EXPECT_EXPR(kl_profiler_end_block(&fx.prof));

  // The point is still pending; clobber the source and make sure the label
  // that reaches the file is the original.
  memset(buffer, 'X', sizeof(buffer) - 1);
  buffer[sizeof(buffer) - 1] = 0;

  kl_profiler_destroy(&fx.prof);

  KL_TestProfLine lines[1];
  KL_TEST_EXPECT_EQ_INT((int)kl_test_prof_read(fx.path, lines, 1), 1);
  KL_TEST_EXPECT_EQ_STR(KL_TEST_PROF_CSTR(lines[0].label), KL_STR_CONST_LIT("dynamic_42"));

  kl_test_prof_teardown(&fx);
}

void test_profiler_label_is_truncated_to_cap() {
  KL_TestProf fx = {0};
  KL_TEST_EXPECT_EXPR(kl_test_prof_setup(&fx, "label_truncate", (KL_ProfilerConfig){0}));

  char big[KL_PROFILER_LABEL_CAP * 2 + 1];
  memset(big, 'a', sizeof(big) - 1);
  big[sizeof(big) - 1] = 0;

  KL_TEST_EXPECT_EXPR(kl_profiler_begin_block(&fx.prof, (KL_StrConst){ .value = big, .length = (int64_t)strlen(big) }));
  KL_TEST_EXPECT_EXPR(kl_profiler_end_block(&fx.prof));
  kl_profiler_destroy(&fx.prof);

  KL_TestProfLine lines[1];
  KL_TEST_EXPECT_EQ_INT((int)kl_test_prof_read(fx.path, lines, 1), 1);
  KL_TEST_EXPECT_EQ_INT((int)strlen(lines[0].label), KL_PROFILER_LABEL_CAP - 1);
  KL_TEST_EXPECT_EXPR(strspn(lines[0].label, "a") == strlen(lines[0].label));

  kl_test_prof_teardown(&fx);
}

void test_profiler_label_is_json_escaped() {
  KL_TestProf fx = {0};
  KL_TEST_EXPECT_EXPR(kl_test_prof_setup(&fx, "label_escape", (KL_ProfilerConfig){0}));

  KL_TEST_EXPECT_EXPR(kl_profiler_begin_block(&fx.prof, KL_STR_CONST_LIT("say \"hi\"\\tab\tnl\n")));
  KL_TEST_EXPECT_EXPR(kl_profiler_end_block(&fx.prof));
  kl_profiler_destroy(&fx.prof);

  KL_TestProfLine lines[4];
  // Still exactly one line: the newline in the label must not split the record.
  KL_TEST_EXPECT_EQ_INT((int)kl_test_prof_read(fx.path, lines, 4), 1);
  KL_TEST_EXPECT_EQ_STR(KL_TEST_PROF_CSTR(lines[0].label), KL_STR_CONST_LIT("say \\\"hi\\\"\\\\tab\\tnl\\n"));

  kl_test_prof_teardown(&fx);
}

void test_profiler_nesting_beyond_max_depth_stays_balanced() {
  KL_TestProf fx = {0};
  KL_TEST_EXPECT_EXPR(kl_test_prof_setup(&fx, "max_depth", (KL_ProfilerConfig){ .max_block_depth = 2 }));

  KL_TEST_EXPECT_EXPR(kl_profiler_begin_block(&fx.prof, KL_STR_CONST_LIT("d0")));
  KL_TEST_EXPECT_EXPR(kl_profiler_begin_block(&fx.prof, KL_STR_CONST_LIT("d1")));
  KL_TEST_ASSERT(!kl_profiler_begin_block(&fx.prof, KL_STR_CONST_LIT("d2")), "third level is not recorded");
  KL_TEST_ASSERT(!kl_profiler_begin_block(&fx.prof, KL_STR_CONST_LIT("d3")), "fourth level is not recorded");
  KL_TEST_EXPECT_EQ_INT((int)kl_profiler_depth(&fx.prof), 4); // logical depth keeps counting

  KL_TEST_ASSERT(!kl_profiler_end_block(&fx.prof), "d3: nothing to submit");
  KL_TEST_ASSERT(!kl_profiler_end_block(&fx.prof), "d2: nothing to submit");
  KL_TEST_EXPECT_EQ_INT((int)kl_profiler_depth(&fx.prof), 2);
  KL_TEST_EXPECT_EXPR(kl_profiler_end_block(&fx.prof)); // d1
  KL_TEST_EXPECT_EXPR(kl_profiler_end_block(&fx.prof)); // d0
  KL_TEST_EXPECT_EQ_INT((int)kl_profiler_depth(&fx.prof), 0);

  KL_TEST_EXPECT_EQ_INT((int)atomic_load(&fx.prof.stats.submitted), 2);
  kl_profiler_destroy(&fx.prof);

  KL_TestProfLine lines[4];
  KL_TEST_EXPECT_EQ_INT((int)kl_test_prof_read(fx.path, lines, 4), 2);
  KL_TEST_EXPECT_EQ_STR(KL_TEST_PROF_CSTR(lines[0].label), KL_STR_CONST_LIT("d1"));
  KL_TEST_EXPECT_EQ_INT(lines[0].depth, 1);
  KL_TEST_EXPECT_EQ_STR(KL_TEST_PROF_CSTR(lines[1].label), KL_STR_CONST_LIT("d0"));
  KL_TEST_EXPECT_EQ_INT(lines[1].depth, 0);

  kl_test_prof_teardown(&fx);
}

void test_profiler_full_queue_drops_points() {
  KL_TestProf fx = {0};
  KL_TEST_EXPECT_EXPR(kl_test_prof_setup(&fx, "queue_full", (KL_ProfilerConfig){ .max_queue_size = 3 }));

  // No writer running, so nothing drains the pending buffer.
  for (int i = 0; i < 5; i++) {
    KL_TEST_EXPECT_EXPR(kl_profiler_begin_block(&fx.prof, KL_STR_CONST_LIT("b")));
    bool ok = kl_profiler_end_block(&fx.prof);
    KL_TEST_EXPECT_EQ_INT(ok, i < 3);
  }
  KL_TEST_EXPECT_EQ_INT((int)kl_profiler_depth(&fx.prof), 0);
  KL_TEST_EXPECT_EQ_INT((int)atomic_load(&fx.prof.stats.submitted), 3);
  KL_TEST_EXPECT_EQ_INT((int)atomic_load(&fx.prof.stats.dropped), 2);
  KL_TEST_EXPECT_EQ_INT((int)fx.prof.messages.pending.length, 3);

  kl_profiler_destroy(&fx.prof);
  KL_TEST_EXPECT_EQ_INT((int)kl_test_prof_line_count(fx.path), 3);

  kl_test_prof_teardown(&fx);
}

void test_profiler_drained_queue_accepts_again() {
  KL_TestProf fx = {0};
  KL_TEST_EXPECT_EXPR(kl_test_prof_setup(&fx, "queue_reuse", (KL_ProfilerConfig){ .max_queue_size = 2, .poll_interval_us = 50 }));
  KL_TEST_EXPECT_EXPR(kl_profiler_start(&fx.prof));

  // Fill, flush, fill again: the buffer is reusable once the writer swapped it out.
  for (int round = 0; round < 3; round++) {
    for (int i = 0; i < 2; i++) {
      KL_TEST_EXPECT_EXPR(kl_profiler_begin_block(&fx.prof, KL_STR_CONST_LIT("b")));
      KL_TEST_EXPECT_EXPR(kl_profiler_end_block(&fx.prof));
    }
    KL_TEST_EXPECT_EXPR(kl_profiler_flush(&fx.prof));
    KL_TEST_EXPECT_EQ_INT((int)atomic_load(&fx.prof.stats.written), 2 * (round + 1));
  }
  KL_TEST_EXPECT_EQ_INT((int)atomic_load(&fx.prof.stats.dropped), 0);
  KL_TEST_EXPECT_EQ_INT((int)kl_test_prof_line_count(fx.path), 6);

  kl_test_prof_teardown(&fx);
}

// ---------- writer thread ----------

void test_profiler_flush_without_writer_returns_false() {
  KL_TestProf fx = {0};
  KL_TEST_EXPECT_EXPR(kl_test_prof_setup(&fx, "flush_no_writer", (KL_ProfilerConfig){0}));

  KL_TEST_EXPECT_EXPR(kl_profiler_begin_block(&fx.prof, KL_STR_CONST_LIT("b")));
  KL_TEST_EXPECT_EXPR(kl_profiler_end_block(&fx.prof));
  KL_TEST_ASSERT(!kl_profiler_flush(&fx.prof), "flush without a writer thread returns false");
  KL_TEST_EXPECT_EQ_INT((int)atomic_load(&fx.prof.stats.written), 0);

  kl_test_prof_teardown(&fx);
}

void test_profiler_flush_waits_for_writes() {
  KL_TestProf fx = {0};
  KL_TEST_EXPECT_EXPR(kl_test_prof_setup(&fx, "flush", (KL_ProfilerConfig){0}));
  KL_TEST_EXPECT_EXPR(kl_profiler_start(&fx.prof));

  enum { COUNT = 200 };
  for (int i = 0; i < COUNT; i++) {
    KL_TEST_EXPECT_EXPR(kl_profiler_begin_block(&fx.prof, KL_STR_CONST_LIT("b")));
    KL_TEST_EXPECT_EXPR(kl_profiler_end_block(&fx.prof));
  }
  KL_TEST_EXPECT_EXPR(kl_profiler_flush(&fx.prof));
  KL_TEST_EXPECT_EQ_INT((int)atomic_load(&fx.prof.stats.written), COUNT);
  // The writer swaps the buffers under this lock, so peek under it too.
  kl_mutex_lock(&fx.prof.messages.lock);
  int64_t pending_length = fx.prof.messages.pending.length;
  kl_mutex_unlock(&fx.prof.messages.lock);
  KL_TEST_EXPECT_EQ_INT((int)pending_length, 0);
  // Flushed while the profiler is still alive: the bytes must already be in the file.
  KL_TEST_EXPECT_EQ_INT((int)kl_test_prof_line_count(fx.path), COUNT);

  // Nothing new: flush returns immediately.
  KL_TEST_EXPECT_EXPR(kl_profiler_flush(&fx.prof));
  KL_TEST_EXPECT_EQ_INT((int)atomic_load(&fx.prof.stats.written), COUNT);

  kl_test_prof_teardown(&fx);
}

void test_profiler_kill_writes_everything_submitted() {
  KL_TestProf fx = {0};
  KL_TEST_EXPECT_EXPR(kl_test_prof_setup(&fx, "kill_drains", (KL_ProfilerConfig){ .poll_interval_us = 100000 }));
  KL_TEST_EXPECT_EXPR(kl_profiler_start(&fx.prof));

  // A long poll interval: the writer is most likely asleep when kill is called,
  // so the final drain is what gets these to disk.
  enum { COUNT = 25 };
  for (int i = 0; i < COUNT; i++) {
    KL_TEST_EXPECT_EXPR(kl_profiler_begin_block(&fx.prof, KL_STR_CONST_LIT("b")));
    KL_TEST_EXPECT_EXPR(kl_profiler_end_block(&fx.prof));
  }
  kl_profiler_kill(&fx.prof);
  KL_TEST_EXPECT_EQ_INT((int)atomic_load(&fx.prof.stats.written), COUNT);
  KL_TEST_EXPECT_EQ_INT((int)kl_test_prof_line_count(fx.path), COUNT);

  kl_test_prof_teardown(&fx);
}

void test_profiler_restart_after_kill_appends() {
  KL_TestProf fx = {0};
  KL_TEST_EXPECT_EXPR(kl_test_prof_setup(&fx, "restart", (KL_ProfilerConfig){0}));

  KL_TEST_EXPECT_EXPR(kl_profiler_start(&fx.prof));
  KL_TEST_EXPECT_EXPR(kl_profiler_begin_block(&fx.prof, KL_STR_CONST_LIT("first")));
  KL_TEST_EXPECT_EXPR(kl_profiler_end_block(&fx.prof));
  kl_profiler_kill(&fx.prof);

  // Recorded while stopped: sits in pending until the next writer.
  KL_TEST_EXPECT_EXPR(kl_profiler_begin_block(&fx.prof, KL_STR_CONST_LIT("stopped")));
  KL_TEST_EXPECT_EXPR(kl_profiler_end_block(&fx.prof));
  KL_TEST_EXPECT_EQ_INT((int)atomic_load(&fx.prof.stats.written), 1);

  KL_TEST_EXPECT_EXPR(kl_profiler_start(&fx.prof));
  KL_TEST_EXPECT_EXPR(kl_profiler_begin_block(&fx.prof, KL_STR_CONST_LIT("second")));
  KL_TEST_EXPECT_EXPR(kl_profiler_end_block(&fx.prof));
  KL_TEST_EXPECT_EXPR(kl_profiler_flush(&fx.prof));
  KL_TEST_EXPECT_EQ_INT((int)atomic_load(&fx.prof.stats.written), 3);

  KL_TestProfLine lines[3];
  KL_TEST_EXPECT_EQ_INT((int)kl_test_prof_read(fx.path, lines, 3), 3);
  KL_TEST_EXPECT_EQ_STR(KL_TEST_PROF_CSTR(lines[0].label), KL_STR_CONST_LIT("first"));
  KL_TEST_EXPECT_EQ_STR(KL_TEST_PROF_CSTR(lines[1].label), KL_STR_CONST_LIT("stopped"));
  KL_TEST_EXPECT_EQ_STR(KL_TEST_PROF_CSTR(lines[2].label), KL_STR_CONST_LIT("second"));

  kl_test_prof_teardown(&fx);
}

void test_profiler_destroy_stops_writer_and_drains() {
  KL_TestProf fx = {0};
  KL_TEST_EXPECT_EXPR(kl_test_prof_setup(&fx, "destroy_running", (KL_ProfilerConfig){ .poll_interval_us = 100000 }));
  KL_TEST_EXPECT_EXPR(kl_profiler_start(&fx.prof));

  enum { COUNT = 30 };
  for (int i = 0; i < COUNT; i++) {
    KL_TEST_EXPECT_EXPR(kl_profiler_begin_block(&fx.prof, KL_STR_CONST_LIT("b")));
    KL_TEST_EXPECT_EXPR(kl_profiler_end_block(&fx.prof));
  }
  // No kill, no flush: destroy has to do both.
  kl_profiler_destroy(&fx.prof);
  KL_TEST_ASSERT(!fx.prof.initialized, "destroyed");
  KL_TEST_EXPECT_EQ_INT((int)kl_test_prof_line_count(fx.path), COUNT);

  kl_test_prof_teardown(&fx);
}

// ---------- threads ----------

typedef struct {
  KL_Profiler* prof;
  int index;
  int64_t iterations;
  int64_t nested_per_iteration;
  char label[32];
  unsigned long long thread_id;
  // observations made on the worker thread, checked after join
  int64_t submitted;
  int64_t rejected;
  int64_t depth_mismatches;
  int64_t sleep_us;
} KL_TestProfWorker;

static void* kl_test_prof_worker_main(void* arg) {
  KL_TestProfWorker* w = (KL_TestProfWorker*)arg;
  w->thread_id = (unsigned long long)pthread_self();
  KL_StrConst label = { .value = w->label, .length = (int64_t)strlen(w->label) };

  for (int64_t i = 0; i < w->iterations; i++) {
    // Nest `nested_per_iteration` deep, checking the depth is *this* thread's.
    for (int64_t d = 0; d < w->nested_per_iteration; d++) {
      if (kl_profiler_depth(w->prof) != d) w->depth_mismatches++;
      if (!kl_profiler_begin_block(w->prof, label)) w->rejected++;
    }
    if (w->sleep_us > 0) usleep((useconds_t)w->sleep_us);
    for (int64_t d = w->nested_per_iteration; d > 0; d--) {
      if (kl_profiler_depth(w->prof) != d) w->depth_mismatches++;
      if (kl_profiler_end_block(w->prof)) w->submitted++;
      else w->rejected++;
    }
    if (kl_profiler_depth(w->prof) != 0) w->depth_mismatches++;
  }
  return 0;
}

void test_profiler_threads_have_independent_stacks() {
  KL_TestProf fx = {0};
  KL_TEST_EXPECT_EXPR(kl_test_prof_setup(&fx, "threads_stacks", (KL_ProfilerConfig){ .max_queue_size = 1 << 16 }));
  KL_TEST_EXPECT_EXPR(kl_profiler_start(&fx.prof));

  enum { THREADS = 8, ITERATIONS = 200, NESTED = 3 };
  KL_TestProfWorker workers[THREADS] = {0};
  pthread_t threads[THREADS];
  for (int i = 0; i < THREADS; i++) {
    workers[i] = (KL_TestProfWorker){ .prof = &fx.prof, .index = i, .iterations = ITERATIONS, .nested_per_iteration = NESTED };
    snprintf(workers[i].label, sizeof(workers[i].label), "worker_%d", i);
    KL_TEST_EXPECT_EQ_INT(pthread_create(&threads[i], 0, kl_test_prof_worker_main, &workers[i]), 0);
  }

  // The main thread interleaves its own blocks: its stack is separate too.
  for (int i = 0; i < ITERATIONS; i++) {
    KL_TEST_EXPECT_EXPR(kl_profiler_begin_block(&fx.prof, KL_STR_CONST_LIT("main")));
    KL_TEST_EXPECT_EQ_INT((int)kl_profiler_depth(&fx.prof), 1);
    KL_TEST_EXPECT_EXPR(kl_profiler_end_block(&fx.prof));
  }

  for (int i = 0; i < THREADS; i++) pthread_join(threads[i], 0);

  int64_t expected_total = ITERATIONS; // main
  for (int i = 0; i < THREADS; i++) {
    KL_TEST_EXPECT_EQ_INT((int)workers[i].depth_mismatches, 0);
    KL_TEST_EXPECT_EQ_INT((int)workers[i].rejected, 0);
    KL_TEST_EXPECT_EQ_INT((int)workers[i].submitted, ITERATIONS * NESTED);
    expected_total += workers[i].submitted;
  }
  KL_TEST_EXPECT_EQ_INT((int)atomic_load(&fx.prof.stats.dropped), 0);
  KL_TEST_EXPECT_EQ_INT((int)atomic_load(&fx.prof.stats.submitted), (int)expected_total);

  KL_TEST_EXPECT_EXPR(kl_profiler_flush(&fx.prof));
  KL_TEST_EXPECT_EQ_INT((int)atomic_load(&fx.prof.stats.written), (int)expected_total);

  KL_TestProfLine* lines = (KL_TestProfLine*)calloc((size_t)expected_total, sizeof(KL_TestProfLine));
  KL_TEST_EXPECT_EQ_INT((int)kl_test_prof_read(fx.path, lines, expected_total), (int)expected_total);

  KL_TEST_EXPECT_EQ_INT((int)kl_test_prof_count_label(lines, expected_total, "main"), ITERATIONS);
  for (int i = 0; i < THREADS; i++) {
    KL_TEST_EXPECT_EQ_INT((int)kl_test_prof_count_label(lines, expected_total, workers[i].label), ITERATIONS * NESTED);
  }

  // Every point carries the id of the thread that recorded it, and the depths
  // of a worker's points are exactly 0..NESTED-1 in equal amounts.
  for (int i = 0; i < THREADS; i++) {
    int64_t per_depth[NESTED] = {0};
    for (int64_t k = 0; k < expected_total; k++) {
      if (strcmp(lines[k].label, workers[i].label) != 0) continue;
      KL_TEST_EXPECT_EXPR(lines[k].thread == workers[i].thread_id);
      KL_TEST_EXPECT_EXPR(lines[k].depth >= 0 && lines[k].depth < NESTED);
      if (lines[k].depth >= 0 && lines[k].depth < NESTED) per_depth[lines[k].depth]++;
    }
    for (int d = 0; d < NESTED; d++) KL_TEST_EXPECT_EQ_INT((int)per_depth[d], ITERATIONS);
  }
  for (int64_t k = 0; k < expected_total; k++) {
    if (strcmp(lines[k].label, "main") == 0) {
      KL_TEST_EXPECT_EXPR(lines[k].thread == (unsigned long long)pthread_self());
      KL_TEST_EXPECT_EQ_INT(lines[k].depth, 0);
    }
  }

  free(lines);
  kl_test_prof_teardown(&fx);
}

void test_profiler_threads_stress_with_small_queue() {
  KL_TestProf fx = {0};
  KL_TEST_EXPECT_EXPR(kl_test_prof_setup(&fx, "threads_stress", (KL_ProfilerConfig){
    .max_queue_size = 64,
    .poll_interval_us = 10,
  }));
  KL_TEST_EXPECT_EXPR(kl_profiler_start(&fx.prof));

  enum { THREADS = 12, ITERATIONS = 5000, NESTED = 2 };
  KL_TestProfWorker workers[THREADS] = {0};
  pthread_t threads[THREADS];
  for (int i = 0; i < THREADS; i++) {
    workers[i] = (KL_TestProfWorker){ .prof = &fx.prof, .index = i, .iterations = ITERATIONS, .nested_per_iteration = NESTED };
    snprintf(workers[i].label, sizeof(workers[i].label), "w%d", i);
    KL_TEST_EXPECT_EQ_INT(pthread_create(&threads[i], 0, kl_test_prof_worker_main, &workers[i]), 0);
  }
  for (int i = 0; i < THREADS; i++) pthread_join(threads[i], 0);

  // With a tiny buffer some points are dropped; the accounting must still
  // balance exactly and stacks must stay consistent under contention.
  int64_t submitted = 0, rejected = 0;
  for (int i = 0; i < THREADS; i++) {
    KL_TEST_EXPECT_EQ_INT((int)workers[i].depth_mismatches, 0);
    submitted += workers[i].submitted;
    rejected += workers[i].rejected;
  }
  KL_TEST_EXPECT_EQ_INT((int)(submitted + rejected), THREADS * ITERATIONS * NESTED);
  KL_TEST_EXPECT_EQ_INT((int)atomic_load(&fx.prof.stats.submitted), (int)submitted);
  KL_TEST_EXPECT_EQ_INT((int)atomic_load(&fx.prof.stats.dropped), (int)rejected);
  KL_TEST_EXPECT_EXPR(submitted > 0);

  kl_profiler_kill(&fx.prof);
  KL_TEST_EXPECT_EQ_INT((int)atomic_load(&fx.prof.stats.written), (int)submitted);
  KL_TEST_EXPECT_EQ_INT((int)kl_test_prof_line_count(fx.path), (int)submitted);

  kl_test_prof_teardown(&fx);
}

void test_profiler_thread_exit_releases_stack() {
  KL_TestProf fx = {0};
  KL_TEST_EXPECT_EXPR(kl_test_prof_setup(&fx, "thread_exit", (KL_ProfilerConfig){0}));

  KL_TEST_EXPECT_EXPR(kl_profiler_begin_block(&fx.prof, KL_STR_CONST_LIT("main")));
  KL_TEST_EXPECT_EXPR(kl_profiler_end_block(&fx.prof));
  KL_TEST_EXPECT_EQ_INT((int)fx.prof.stacks.pool.taken_indices.length, 1); // main's stack

  enum { ROUNDS = 5 };
  for (int r = 0; r < ROUNDS; r++) {
    KL_TestProfWorker w = { .prof = &fx.prof, .iterations = 3, .nested_per_iteration = 2 };
    snprintf(w.label, sizeof(w.label), "short_lived");
    pthread_t t;
    KL_TEST_EXPECT_EQ_INT(pthread_create(&t, 0, kl_test_prof_worker_main, &w), 0);
    pthread_join(t, 0);
    KL_TEST_EXPECT_EQ_INT((int)w.depth_mismatches, 0);
    KL_TEST_EXPECT_EQ_INT((int)w.submitted, 6);
    // The exited thread's stack went back to the pool (key destructor).
    KL_TEST_EXPECT_EQ_INT((int)fx.prof.stacks.pool.taken_indices.length, 1);
  }
  // Every round reused the same slot, so the pool never had to grow.
  KL_TEST_EXPECT_EQ_INT((int)kl_binary_pool_capacity(&fx.prof.stacks.pool), (int)fx.prof.config.max_threads);

  kl_profiler_destroy(&fx.prof);
  KL_TEST_EXPECT_EQ_INT((int)kl_test_prof_line_count(fx.path), 1 + ROUNDS * 6);

  kl_test_prof_teardown(&fx);
}

void test_profiler_stack_pool_grows_beyond_max_threads() {
  KL_TestProf fx = {0};
  KL_TEST_EXPECT_EXPR(kl_test_prof_setup(&fx, "pool_grow", (KL_ProfilerConfig){ .max_threads = 2, .max_queue_size = 1 << 12 }));
  KL_TEST_EXPECT_EXPR(kl_profiler_start(&fx.prof));

  // More simultaneously-live threads than `max_threads`: each holds an open
  // block (and thus a stack) across a sleep so they all overlap.
  enum { THREADS = 7 };
  KL_TestProfWorker workers[THREADS] = {0};
  pthread_t threads[THREADS];
  for (int i = 0; i < THREADS; i++) {
    workers[i] = (KL_TestProfWorker){ .prof = &fx.prof, .index = i, .iterations = 4, .nested_per_iteration = 1, .sleep_us = 5000 };
    snprintf(workers[i].label, sizeof(workers[i].label), "t%d", i);
    KL_TEST_EXPECT_EQ_INT(pthread_create(&threads[i], 0, kl_test_prof_worker_main, &workers[i]), 0);
  }
  for (int i = 0; i < THREADS; i++) pthread_join(threads[i], 0);

  for (int i = 0; i < THREADS; i++) {
    KL_TEST_EXPECT_EQ_INT((int)workers[i].rejected, 0);
    KL_TEST_EXPECT_EQ_INT((int)workers[i].submitted, 4);
    KL_TEST_EXPECT_EQ_INT((int)workers[i].depth_mismatches, 0);
  }
  KL_TEST_EXPECT_EXPR(kl_binary_pool_capacity(&fx.prof.stacks.pool) >= THREADS);
  KL_TEST_EXPECT_EQ_INT((int)(kl_binary_pool_capacity(&fx.prof.stacks.pool) % 2), 0); // grew in chunks of max_threads

  KL_TEST_EXPECT_EXPR(kl_profiler_flush(&fx.prof));
  KL_TEST_EXPECT_EQ_INT((int)kl_test_prof_line_count(fx.path), THREADS * 4);

  kl_test_prof_teardown(&fx);
}

void test_profiler_two_profilers_are_independent() {
  KL_TestProf a = {0};
  KL_TestProf b = {0};
  KL_TEST_EXPECT_EXPR(kl_test_prof_setup(&a, "two_a", (KL_ProfilerConfig){0}));
  KL_TEST_EXPECT_EXPR(kl_test_prof_setup(&b, "two_b", (KL_ProfilerConfig){0}));

  // Interleaved but properly nested per profiler: each keeps its own stack.
  KL_TEST_EXPECT_EXPR(kl_profiler_begin_block(&a.prof, KL_STR_CONST_LIT("a_outer")));
  KL_TEST_EXPECT_EXPR(kl_profiler_begin_block(&b.prof, KL_STR_CONST_LIT("b_only")));
  KL_TEST_EXPECT_EXPR(kl_profiler_begin_block(&a.prof, KL_STR_CONST_LIT("a_inner")));
  KL_TEST_EXPECT_EQ_INT((int)kl_profiler_depth(&a.prof), 2);
  KL_TEST_EXPECT_EQ_INT((int)kl_profiler_depth(&b.prof), 1);

  KL_TEST_EXPECT_EXPR(kl_profiler_end_block(&b.prof));
  KL_TEST_EXPECT_EQ_INT((int)kl_profiler_depth(&a.prof), 2);
  KL_TEST_EXPECT_EQ_INT((int)kl_profiler_depth(&b.prof), 0);
  KL_TEST_ASSERT(!kl_profiler_end_block(&b.prof), "b has nothing left to end");

  KL_TEST_EXPECT_EXPR(kl_profiler_end_block(&a.prof));
  KL_TEST_EXPECT_EXPR(kl_profiler_end_block(&a.prof));
  KL_TEST_EXPECT_EQ_INT((int)kl_profiler_depth(&a.prof), 0);

  kl_profiler_destroy(&a.prof);
  kl_profiler_destroy(&b.prof);

  KL_TestProfLine lines[4];
  KL_TEST_EXPECT_EQ_INT((int)kl_test_prof_read(a.path, lines, 4), 2);
  KL_TEST_EXPECT_EQ_STR(KL_TEST_PROF_CSTR(lines[0].label), KL_STR_CONST_LIT("a_inner"));
  KL_TEST_EXPECT_EQ_STR(KL_TEST_PROF_CSTR(lines[1].label), KL_STR_CONST_LIT("a_outer"));
  KL_TEST_EXPECT_EQ_INT((int)kl_test_prof_read(b.path, lines, 4), 1);
  KL_TEST_EXPECT_EQ_STR(KL_TEST_PROF_CSTR(lines[0].label), KL_STR_CONST_LIT("b_only"));

  kl_test_prof_teardown(&a);
  kl_test_prof_teardown(&b);
}
