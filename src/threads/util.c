#define _GNU_SOURCE
#include <kl/threads/util.h>
#include <unistd.h>
#include <stdio.h>
#include <assert.h>

#include <sched.h>

long kl_nprocs_online() {
  return sysconf(_SC_NPROCESSORS_ONLN);
}

long kl_nprocs_configured() {
  return sysconf(_SC_NPROCESSORS_CONF);
}

long kl_nprocs() {
  cpu_set_t set = {0};
  
  if (sched_getaffinity(0, sizeof(set), &set) == 0) {
    return CPU_COUNT(&set);
  }

  return kl_nprocs_online();
}
