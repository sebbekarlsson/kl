#include <pthread.h>
#include <kl/threads/mutex.h>
#include <assert.h>

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
