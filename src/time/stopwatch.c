#include <kl/time/stopwatch.h>

static inline double get_time_now(clockid_t clock_id) {
  struct timespec now = {0};
  clock_gettime(clock_id, &now);
  return ((double)now.tv_nsec / 1000000000.0) + ((double)now.tv_sec);
}

double kl_stopwatch_start(KL_StopWatch* watch, clockid_t id) {
  watch->id = id;
  watch->start = get_time_now(id);
  watch->finish = 0;
  return watch->start;
}

double kl_stopwatch_now(KL_StopWatch* watch) {
  return get_time_now(watch->id);
}

double kl_stopwatch_elapsed(KL_StopWatch* watch) {
  return get_time_now(watch->id) - watch->start;
}

double kl_stopwatch_stop(KL_StopWatch* watch) {
  watch->finish = get_time_now(watch->id);
  double elapsed = watch->finish - watch->start;
  watch->start = 0;
  return elapsed;
}
