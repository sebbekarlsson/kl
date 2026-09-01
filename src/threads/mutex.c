#define _GNU_SOURCE /* pthread_mutex_clocklock */
#include <pthread.h>
#include <kl/threads/mutex.h>
#include <assert.h>
#include <stdint.h>
#include <time.h>

bool kl_mutex_init(KL_Mutex* mutex) {
  if (mutex->initialized) return true;

  if (pthread_mutexattr_init(&mutex->attr) != 0) {
    return false;
  }

  if (pthread_mutexattr_settype(&mutex->attr, PTHREAD_MUTEX_RECURSIVE) != 0) {
    return false;
  }

  if (pthread_mutex_init(&mutex->id, &mutex->attr) != 0) {
    return false;
  }

  mutex->initialized = true;
  return true;
}

int kl_mutex_lock(KL_Mutex* mutex) {
  assert(mutex->initialized == true);
  return pthread_mutex_lock(&mutex->id);
}

bool kl_mutex_trylock(KL_Mutex* mutex) {
  assert(mutex->initialized == true);
  return pthread_mutex_trylock(&mutex->id) == 0;
}


bool kl_mutex_trylock_timeout(KL_Mutex* mutex, double timeout_ms) {
  assert(mutex->initialized == true);

  /* A non-positive (or NaN) timeout degrades to a plain trylock. */
  if (!(timeout_ms > 0.0)) {
    return pthread_mutex_trylock(&mutex->id) == 0;
  }

  /* pthread_mutex_clocklock wants an *absolute* deadline, so take "now" and
   * add the relative timeout to it. CLOCK_MONOTONIC makes the wait immune to
   * wall-clock jumps (NTP steps, manual date changes). */
  struct timespec deadline;
  if (clock_gettime(CLOCK_MONOTONIC, &deadline) != 0) {
    return pthread_mutex_trylock(&mutex->id) == 0;
  }

  const int64_t NS_PER_SEC = 1000000000LL;
  int64_t timeout_ns = (int64_t)(timeout_ms * 1000000.0);

  int64_t ns = (int64_t)deadline.tv_nsec + timeout_ns;
  deadline.tv_sec += (time_t)(ns / NS_PER_SEC);
  deadline.tv_nsec = (long)(ns % NS_PER_SEC);

  return pthread_mutex_clocklock(&mutex->id, CLOCK_MONOTONIC, &deadline) == 0;
}

int kl_mutex_unlock(KL_Mutex* mutex) {
  assert(mutex->initialized == true);
  return pthread_mutex_unlock(&mutex->id);
}

void kl_mutex_destroy(KL_Mutex* mutex) {
  assert(mutex->initialized == true);
  pthread_mutex_destroy(&mutex->id);
  pthread_mutexattr_destroy(&mutex->attr);
  mutex->initialized = false;
}
