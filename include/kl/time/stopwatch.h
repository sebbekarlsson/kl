#ifndef KL_STOPWATCH_H
#define KL_STOPWATCH_H
#include <time.h>

typedef struct {
  double start;
  double finish;
  clockid_t id;
} KL_StopWatch;

/**
  @param id - for example CLOCK_THREAD_CPUTIME_ID from <time.h>
**/
double kl_stopwatch_start(KL_StopWatch* watch, clockid_t id);

double kl_stopwatch_now(KL_StopWatch* watch);

double kl_stopwatch_elapsed(KL_StopWatch* watch);

double kl_stopwatch_stop(KL_StopWatch* watch);

#endif
