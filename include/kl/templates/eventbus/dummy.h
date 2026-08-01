#ifndef KL_EVENTBUS_H
#define KL_EVENTBUS_H

#include <kl/templates/eventbus/shared.h>
#include <kl/templates/x__shared.h>
#include <kl/templates/list/list.h>
#include <kl/templates/list/int64_list.h>
#include <kl/templates/hashmap/hashmap.h>
#include <kl/templates/hashmap/int64_list_hashmap.h>
#include <kl/templates/pool/binary_pool.h>
#include <kl/memory/allocator.h>

#include <stdint.h>
#include <stdbool.h>


typedef struct {
  T_X type;
  T_Y payload;
} KL_NAME_EventBusEvent;

// event callback
typedef KL_EventBusSignal (*KL_NAME_EventBusListenerFunction)(void* user_ptr, const KL_NAME_EventBusEvent* event);

typedef struct {
  KL_NAME_EventBusListenerFunction fn;
  void* user_ptr;
} KL_NAME_EventBusListener;

/* what the subscriber pool stores; event_type/is_global let unsub find the
   list a subscriber id lives in */
typedef struct {
  KL_NAME_EventBusListener listener;
  T_X event_type;
  bool is_global;
} KL_NAME_EventBusSubscriber;

typedef struct {
  bool initialized;
  KL_EventBusConfig config;
  /* event_type -> list of subscriber id's */
  KL_int64_list_Hashmap subscriber_lists;
  /* subscribers that subscribe to all events */
  KL_int64_List global_subscribers;
  /* allocates `KL_NAME_EventBusSubscriber` */
  KL_BinaryPool subscriber_pool;
  /* > 0 while inside emit (counts nested emits from callbacks) */
  int64_t emit_depth;
  /* unsubs requested during emit; flushed when the outermost emit returns */
  KL_int64_List pending_unsubs;
} KL_NAME_EventBus;

void kl_NAME_eventbus_init(KL_NAME_EventBus* bus, KL_EventBusConfig cfg);

void kl_NAME_eventbus_destroy(KL_NAME_EventBus* bus);

 KL_EventBusSignal kl_NAME_eventbus_emit(KL_NAME_EventBus* bus, const KL_NAME_EventBusEvent* event);

// returns the subscriber id
int64_t kl_NAME_eventbus_sub(KL_NAME_EventBus* bus, T_X event_type, KL_NAME_EventBusListener listener);

int64_t kl_NAME_eventbus_sub_all(KL_NAME_EventBus* bus, KL_NAME_EventBusListener listener);

void kl_NAME_eventbus_unsub(KL_NAME_EventBus* bus, int64_t sub_id);

/* safe to call from inside a listener callback: the unsub is queued and applied
   after the current emit finishes. Outside of an emit it unsubs immediately. */
void kl_NAME_eventbus_unsub_deferred(KL_NAME_EventBus* bus, int64_t sub_id);

#endif
