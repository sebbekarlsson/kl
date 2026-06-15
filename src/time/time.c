#include <kl/time/time.h>
#include <time.h>

double kl_time_now_s__monotonic() {
  struct timespec now = {0};
  clock_gettime(CLOCK_MONOTONIC, &now);
  return ((double)now.tv_nsec / 1000000000.0) + ((double)now.tv_sec);
}

double kl_time_now_ms__monotonic() {
  struct timespec now = {0};
  clock_gettime(CLOCK_MONOTONIC, &now);
  return ((double)now.tv_nsec / 1000000.0) + ((double)now.tv_sec * 1000.0);
}
