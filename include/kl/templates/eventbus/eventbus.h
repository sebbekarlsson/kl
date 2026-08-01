/*
This file was automatically generated and must not be modified directly.
*/


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
#include <kl/templates/eventbus/dummy.h>
#include <kl/macros.h>
#include <assert.h>
#include <stdalign.h>
#include <string.h>


#define KL_DEFINE_EVENTBUS(NAME, T, U)                                                                                        \
  typedef struct {                                                                                                            \
    T type;                                                                                                                   \
    U payload;                                                                                                                \
  } KL_##NAME##_EventBusEvent;                                                                                                \
  typedef KL_EventBusSignal (*KL_##NAME##_EventBusListenerFunction)(void* user_ptr, const KL_##NAME##_EventBusEvent* event);  \
  typedef struct {                                                                                                            \
    KL_##NAME##_EventBusListenerFunction fn;                                                                                  \
    void* user_ptr;                                                                                                           \
  } KL_##NAME##_EventBusListener;                                                                                             \
  /* what the subscriber pool stores; event_type/is_global let unsub find the                                                 \
     list a subscriber id lives in */                                                                                         \
  typedef struct {                                                                                                            \
    KL_##NAME##_EventBusListener listener;                                                                                    \
    T event_type;                                                                                                             \
    bool is_global;                                                                                                           \
  } KL_##NAME##_EventBusSubscriber;                                                                                           \
  typedef struct {                                                                                                            \
    bool initialized;                                                                                                         \
    KL_EventBusConfig config;                                                                                                 \
    /* event_type -> list of subscriber id's */                                                                               \
    KL_int64_list_Hashmap subscriber_lists;                                                                                   \
    /* subscribers that subscribe to all events */                                                                            \
    KL_int64_List global_subscribers;                                                                                         \
    /* allocates `KL_##NAME##_EventBusSubscriber` */                                                                          \
    KL_BinaryPool subscriber_pool;                                                                                            \
    /* > 0 while inside emit (counts nested emits from callbacks) */                                                          \
    int64_t emit_depth;                                                                                                       \
    /* unsubs requested during emit; flushed when the outermost emit returns */                                               \
    KL_int64_List pending_unsubs;                                                                                             \
  } KL_##NAME##_EventBus;                                                                                                     \
  void kl_##NAME##_eventbus_init(KL_##NAME##_EventBus* bus, KL_EventBusConfig cfg);                                           \
  void kl_##NAME##_eventbus_destroy(KL_##NAME##_EventBus* bus);                                                               \
   KL_EventBusSignal kl_##NAME##_eventbus_emit(KL_##NAME##_EventBus* bus, const KL_##NAME##_EventBusEvent* event);            \
  int64_t kl_##NAME##_eventbus_sub(KL_##NAME##_EventBus* bus, T event_type, KL_##NAME##_EventBusListener listener);           \
  int64_t kl_##NAME##_eventbus_sub_all(KL_##NAME##_EventBus* bus, KL_##NAME##_EventBusListener listener);                     \
  void kl_##NAME##_eventbus_unsub(KL_##NAME##_EventBus* bus, int64_t sub_id);                                                 \
  /* safe to call from inside a listener callback: the unsub is queued and applied                                            \
     after the current emit finishes. Outside of an emit it unsubs immediately. */                                            \
  void kl_##NAME##_eventbus_unsub_deferred(KL_##NAME##_EventBus* bus, int64_t sub_id);



#define KL_IMPL_EVENTBUS(NAME, T, U)                                                                                                                                     \
  void kl_##NAME##_eventbus_init(KL_##NAME##_EventBus* bus, KL_EventBusConfig cfg) {                                                                                     \
    if (bus->initialized) return;                                                                                                                                        \
    assert(cfg.allocator != 0);                                                                                                                                          \
    if (cfg.listener_capacity <= 0) cfg.listener_capacity = 64;                                                                                                          \
    bus->config = cfg;                                                                                                                                                   \
    kl_int64_list_hashmap_init(&bus->subscriber_lists, (KL_HashmapConfig){                                                                                               \
      .allocator = cfg.allocator,                                                                                                                                        \
      .capacity = KL_MAX(cfg.listener_capacity, 103),                                                                                                                    \
      .collision_capacity = 4,                                                                                                                                           \
      .track_used_slots = true                                                                                                                                           \
    });                                                                                                                                                                  \
    kl_int64_list_init(&bus->global_subscribers, (KL_ListConfig){                                                                                                        \
      .allocator = cfg.allocator,                                                                                                                                        \
      .capacity = 8                                                                                                                                                      \
    });                                                                                                                                                                  \
    bus->emit_depth = 0;                                                                                                                                                 \
    kl_int64_list_init(&bus->pending_unsubs, (KL_ListConfig){                                                                                                            \
      .allocator = cfg.allocator,                                                                                                                                        \
      .capacity = 8                                                                                                                                                      \
    });                                                                                                                                                                  \
    kl_binary_pool_init(&bus->subscriber_pool, (KL_BinaryPoolConfig){                                                                                                    \
      .allocator = cfg.allocator,                                                                                                                                        \
      .count = cfg.listener_capacity,                                                                                                                                    \
      .element_size = (int64_t)sizeof(KL_##NAME##_EventBusSubscriber),                                                                                                   \
      .element_alignment = (int64_t)alignof(KL_##NAME##_EventBusSubscriber)                                                                                              \
    });                                                                                                                                                                  \
    bus->initialized = true;                                                                                                                                             \
  }                                                                                                                                                                      \
  void kl_##NAME##_eventbus_destroy(KL_##NAME##_EventBus* bus) {                                                                                                         \
    assert(bus->initialized);                                                                                                                                            \
    for (int64_t i = 0; i < bus->subscriber_lists.used_slots.length; i++) {                                                                                              \
      int64_t slot_idx = bus->subscriber_lists.used_slots.items[i];                                                                                                      \
      KL_int64_list_HashmapSlot* slot = &bus->subscriber_lists.slots.items[slot_idx];                                                                                    \
      if (slot->value.initialized) {                                                                                                                                     \
        kl_int64_list_destroy(&slot->value);                                                                                                                             \
      }                                                                                                                                                                  \
    }                                                                                                                                                                    \
    kl_int64_list_hashmap_destroy(&bus->subscriber_lists);                                                                                                               \
    kl_int64_list_destroy(&bus->global_subscribers);                                                                                                                     \
    kl_int64_list_destroy(&bus->pending_unsubs);                                                                                                                         \
    kl_binary_pool_destroy(&bus->subscriber_pool);                                                                                                                       \
    memset(bus, 0, sizeof(KL_##NAME##_EventBus));                                                                                                                        \
  }                                                                                                                                                                      \
  static inline KL_EventBusSignal kl_##NAME##_eventbus_dispatch_(KL_##NAME##_EventBus* bus, const KL_int64_List* subscribers, const KL_##NAME##_EventBusEvent* event) {  \
    for (int64_t i = 0; i < subscribers->length; i++) {                                                                                                                  \
      KL_##NAME##_EventBusSubscriber* sub = (KL_##NAME##_EventBusSubscriber*)kl_binary_pool_get(&bus->subscriber_pool, subscribers->items[i]);                           \
      if (sub == 0 || sub->listener.fn == 0) continue;                                                                                                                   \
      if (sub->listener.fn(sub->listener.user_ptr, event) == KL_EVENTBUS_SIGNAL_STOP) {                                                                                  \
        return KL_EVENTBUS_SIGNAL_STOP;                                                                                                                                  \
      }                                                                                                                                                                  \
    }                                                                                                                                                                    \
    return KL_EVENTBUS_SIGNAL_NONE;                                                                                                                                      \
  }                                                                                                                                                                      \
  static inline void kl_##NAME##_eventbus_flush_pending_unsubs_(KL_##NAME##_EventBus* bus) {                                                                             \
    int64_t sub_id = 0;                                                                                                                                                  \
    while (kl_int64_list_pop(&bus->pending_unsubs, &sub_id)) {                                                                                                           \
      kl_##NAME##_eventbus_unsub(bus, sub_id);                                                                                                                           \
    }                                                                                                                                                                    \
  }                                                                                                                                                                      \
  KL_EventBusSignal kl_##NAME##_eventbus_emit(KL_##NAME##_EventBus* bus, const KL_##NAME##_EventBusEvent* event) {                                                       \
    assert(bus->initialized);                                                                                                                                            \
    assert(event != 0);                                                                                                                                                  \
    bus->emit_depth += 1;                                                                                                                                                \
    T event_type = event->type;                                                                                                                                          \
    KL_int64_List* subscribers = 0;                                                                                                                                      \
    KL_EventBusSignal signal = KL_EVENTBUS_SIGNAL_NONE;                                                                                                                  \
    if (kl_int64_list_hashmap_get_ptr(&bus->subscriber_lists, KL_BLOB_AUTO(event_type), &subscribers) && subscribers != 0 && subscribers->initialized) {                 \
      signal = kl_##NAME##_eventbus_dispatch_(bus, subscribers, event);                                                                                                  \
    }                                                                                                                                                                    \
    if (signal != KL_EVENTBUS_SIGNAL_STOP) {                                                                                                                             \
      kl_##NAME##_eventbus_dispatch_(bus, &bus->global_subscribers, event);                                                                                              \
    }                                                                                                                                                                    \
    bus->emit_depth -= 1;                                                                                                                                                \
    assert(bus->emit_depth >= 0);                                                                                                                                        \
    if (bus->emit_depth == 0) {                                                                                                                                          \
      kl_##NAME##_eventbus_flush_pending_unsubs_(bus);                                                                                                                   \
    }                                                                                                                                                                    \
    return signal;                                                                                                                                                       \
  }                                                                                                                                                                      \
  static inline int64_t kl_##NAME##_eventbus_sub_(KL_##NAME##_EventBus* bus, T event_type, bool is_global, KL_##NAME##_EventBusListener listener) {                      \
    assert(bus->initialized);                                                                                                                                            \
    assert(listener.fn != 0);                                                                                                                                            \
    uint8_t* slot_memory = 0;                                                                                                                                            \
    int64_t sub_id = kl_binary_pool_acquire(&bus->subscriber_pool, &slot_memory);                                                                                        \
    if (sub_id < 0) return -1;                                                                                                                                           \
    KL_##NAME##_EventBusSubscriber* sub = (KL_##NAME##_EventBusSubscriber*)slot_memory;                                                                                  \
    sub->listener = listener;                                                                                                                                            \
    sub->event_type = event_type;                                                                                                                                        \
    sub->is_global = is_global;                                                                                                                                          \
    if (is_global) {                                                                                                                                                     \
      kl_int64_list_push(&bus->global_subscribers, sub_id);                                                                                                              \
      return sub_id;                                                                                                                                                     \
    }                                                                                                                                                                    \
    KL_int64_List* subscribers = 0;                                                                                                                                      \
    if (!kl_int64_list_hashmap_get_ptr(&bus->subscriber_lists, KL_BLOB_AUTO(event_type), &subscribers)) {                                                                \
      kl_int64_list_hashmap_set(&bus->subscriber_lists, KL_BLOB_AUTO(event_type), (KL_int64_List){0});                                                                   \
      kl_int64_list_hashmap_get_ptr(&bus->subscriber_lists, KL_BLOB_AUTO(event_type), &subscribers);                                                                     \
    }                                                                                                                                                                    \
    assert(subscribers != 0);                                                                                                                                            \
    if (!subscribers->initialized) {                                                                                                                                     \
      kl_int64_list_init(subscribers, (KL_ListConfig){                                                                                                                   \
        .allocator = bus->config.allocator,                                                                                                                              \
        .capacity = 8                                                                                                                                                    \
      });                                                                                                                                                                \
    }                                                                                                                                                                    \
    kl_int64_list_push(subscribers, sub_id);                                                                                                                             \
    return sub_id;                                                                                                                                                       \
  }                                                                                                                                                                      \
  int64_t kl_##NAME##_eventbus_sub(KL_##NAME##_EventBus* bus, T event_type, KL_##NAME##_EventBusListener listener) {                                                     \
    return kl_##NAME##_eventbus_sub_(bus, event_type, false, listener);                                                                                                  \
  }                                                                                                                                                                      \
  int64_t kl_##NAME##_eventbus_sub_all(KL_##NAME##_EventBus* bus, KL_##NAME##_EventBusListener listener) {                                                               \
    T event_type;                                                                                                                                                        \
    memset(&event_type, 0, sizeof(T));                                                                                                                                   \
    return kl_##NAME##_eventbus_sub_(bus, event_type, true, listener);                                                                                                   \
  }                                                                                                                                                                      \
  void kl_##NAME##_eventbus_unsub(KL_##NAME##_EventBus* bus, int64_t sub_id) {                                                                                           \
    assert(bus->initialized);                                                                                                                                            \
    KL_##NAME##_EventBusSubscriber* sub = (KL_##NAME##_EventBusSubscriber*)kl_binary_pool_get(&bus->subscriber_pool, sub_id);                                            \
    if (sub == 0) return;                                                                                                                                                \
    if (sub->is_global) {                                                                                                                                                \
      int64_t idx = kl_int64_list_index_of(&bus->global_subscribers, sub_id);                                                                                            \
      if (idx >= 0) kl_int64_list_pop_at(&bus->global_subscribers, idx, 0);                                                                                              \
    } else {                                                                                                                                                             \
      T event_type = sub->event_type;                                                                                                                                    \
      KL_int64_List* subscribers = 0;                                                                                                                                    \
      if (kl_int64_list_hashmap_get_ptr(&bus->subscriber_lists, KL_BLOB_AUTO(event_type), &subscribers) && subscribers != 0 && subscribers->initialized) {               \
        int64_t idx = kl_int64_list_index_of(subscribers, sub_id);                                                                                                       \
        if (idx >= 0) kl_int64_list_pop_at(subscribers, idx, 0);                                                                                                         \
      }                                                                                                                                                                  \
    }                                                                                                                                                                    \
    kl_binary_pool_release(&bus->subscriber_pool, sub_id);                                                                                                               \
  }                                                                                                                                                                      \
  void kl_##NAME##_eventbus_unsub_deferred(KL_##NAME##_EventBus* bus, int64_t sub_id) {                                                                                  \
    assert(bus->initialized);                                                                                                                                            \
    if (bus->emit_depth <= 0) {                                                                                                                                          \
      kl_##NAME##_eventbus_unsub(bus, sub_id);                                                                                                                           \
      return;                                                                                                                                                            \
    }                                                                                                                                                                    \
    if (!kl_int64_list_includes(&bus->pending_unsubs, sub_id)) {                                                                                                         \
      kl_int64_list_push(&bus->pending_unsubs, sub_id);                                                                                                                  \
    }                                                                                                                                                                    \
  }

/* generated */
