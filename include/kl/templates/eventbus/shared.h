#ifndef KL_EVENTBUS__SHARED
#define KL_EVENTBUS__SHARED

#include <stdint.h>
#include <kl/memory/allocator.h>

typedef struct {
  int64_t listener_capacity;
  KL_Allocator* allocator; // required
} KL_EventBusConfig;

typedef enum {
  KL_EVENTBUS_SIGNAL_NONE,
  KL_EVENTBUS_SIGNAL_STOP
} KL_EventBusSignal;

#endif
