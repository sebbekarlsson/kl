#ifndef KL_MUTEX_H
#define KL_MUTEX_H
#include <pthread.h>
#include <stdbool.h>

typedef struct {
  pthread_mutex_t id;
  pthread_mutexattr_t attr;
  bool initialized;
} KL_Mutex;

bool kl_mutex_init(KL_Mutex* mutex);

int kl_mutex_lock(KL_Mutex* mutex);

bool kl_mutex_trylock(KL_Mutex* mutex);

bool kl_mutex_trylock_timeout(KL_Mutex* mutex, double timeout_ms);

int kl_mutex_unlock(KL_Mutex* mutex);

void kl_mutex_destroy(KL_Mutex* mutex);

#endif
