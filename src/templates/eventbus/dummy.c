#include <kl/templates/eventbus/dummy.h>
#include <kl/macros.h>
#include <assert.h>
#include <stdalign.h>
#include <string.h>

void kl_NAME_eventbus_init(KL_NAME_EventBus* bus, KL_EventBusConfig cfg) {
  if (bus->initialized) return;
  assert(cfg.allocator != 0);

  if (cfg.listener_capacity <= 0) cfg.listener_capacity = 64;

  bus->config = cfg;

  kl_int64_list_hashmap_init(&bus->subscriber_lists, (KL_HashmapConfig){
    .allocator = cfg.allocator,
    .capacity = KL_MAX(cfg.listener_capacity, 103),
    .collision_capacity = 4,
    .track_used_slots = true
  });

  kl_int64_list_init(&bus->global_subscribers, (KL_ListConfig){
    .allocator = cfg.allocator,
    .capacity = 8
  });

  bus->emit_depth = 0;
  kl_int64_list_init(&bus->pending_unsubs, (KL_ListConfig){
    .allocator = cfg.allocator,
    .capacity = 8
  });

  kl_binary_pool_init(&bus->subscriber_pool, (KL_BinaryPoolConfig){
    .allocator = cfg.allocator,
    .count = cfg.listener_capacity,
    .element_size = (int64_t)sizeof(KL_NAME_EventBusSubscriber),
    .element_alignment = (int64_t)alignof(KL_NAME_EventBusSubscriber)
  });

  bus->initialized = true;
}

void kl_NAME_eventbus_destroy(KL_NAME_EventBus* bus) {
  assert(bus->initialized);

  for (int64_t i = 0; i < bus->subscriber_lists.used_slots.length; i++) {
    int64_t slot_idx = bus->subscriber_lists.used_slots.items[i];
    KL_int64_list_HashmapSlot* slot = &bus->subscriber_lists.slots.items[slot_idx];
    if (slot->value.initialized) {
      kl_int64_list_destroy(&slot->value);
    }
  }
  kl_int64_list_hashmap_destroy(&bus->subscriber_lists);
  kl_int64_list_destroy(&bus->global_subscribers);
  kl_int64_list_destroy(&bus->pending_unsubs);
  kl_binary_pool_destroy(&bus->subscriber_pool);

  memset(bus, 0, sizeof(KL_NAME_EventBus));
}

static inline KL_EventBusSignal kl_NAME_eventbus_dispatch_(KL_NAME_EventBus* bus, const KL_int64_List* subscribers, const KL_NAME_EventBusEvent* event) {
  for (int64_t i = 0; i < subscribers->length; i++) {
    KL_NAME_EventBusSubscriber* sub = (KL_NAME_EventBusSubscriber*)kl_binary_pool_get(&bus->subscriber_pool, subscribers->items[i]);
    if (sub == 0 || sub->listener.fn == 0) continue;
    if (sub->listener.fn(sub->listener.user_ptr, event) == KL_EVENTBUS_SIGNAL_STOP) {
      return KL_EVENTBUS_SIGNAL_STOP;
    }
  }
  return KL_EVENTBUS_SIGNAL_NONE;
}

static inline void kl_NAME_eventbus_flush_pending_unsubs_(KL_NAME_EventBus* bus) {
  int64_t sub_id = 0;
  while (kl_int64_list_pop(&bus->pending_unsubs, &sub_id)) {
    kl_NAME_eventbus_unsub(bus, sub_id);
  }
}

KL_EventBusSignal kl_NAME_eventbus_emit(KL_NAME_EventBus* bus, const KL_NAME_EventBusEvent* event) {
  assert(bus->initialized);
  assert(event != 0);

  bus->emit_depth += 1;

  T_X event_type = event->type;
  KL_int64_List* subscribers = 0;
  KL_EventBusSignal signal = KL_EVENTBUS_SIGNAL_NONE;
  if (kl_int64_list_hashmap_get_ptr(&bus->subscriber_lists, KL_BLOB_AUTO(event_type), &subscribers) && subscribers != 0 && subscribers->initialized) {
    signal = kl_NAME_eventbus_dispatch_(bus, subscribers, event);
  }
  if (signal != KL_EVENTBUS_SIGNAL_STOP) {
    kl_NAME_eventbus_dispatch_(bus, &bus->global_subscribers, event);
  }

  bus->emit_depth -= 1;
  assert(bus->emit_depth >= 0);
  if (bus->emit_depth == 0) {
    kl_NAME_eventbus_flush_pending_unsubs_(bus);
  }

  return signal;
}

static inline int64_t kl_NAME_eventbus_sub_(KL_NAME_EventBus* bus, T_X event_type, bool is_global, KL_NAME_EventBusListener listener) {
  assert(bus->initialized);
  assert(listener.fn != 0);

  uint8_t* slot_memory = 0;
  int64_t sub_id = kl_binary_pool_acquire(&bus->subscriber_pool, &slot_memory);
  if (sub_id < 0) return -1;

  KL_NAME_EventBusSubscriber* sub = (KL_NAME_EventBusSubscriber*)slot_memory;
  sub->listener = listener;
  sub->event_type = event_type;
  sub->is_global = is_global;

  if (is_global) {
    kl_int64_list_push(&bus->global_subscribers, sub_id);
    return sub_id;
  }

  KL_int64_List* subscribers = 0;
  if (!kl_int64_list_hashmap_get_ptr(&bus->subscriber_lists, KL_BLOB_AUTO(event_type), &subscribers)) {
    kl_int64_list_hashmap_set(&bus->subscriber_lists, KL_BLOB_AUTO(event_type), (KL_int64_List){0});
    kl_int64_list_hashmap_get_ptr(&bus->subscriber_lists, KL_BLOB_AUTO(event_type), &subscribers);
  }
  assert(subscribers != 0);

  if (!subscribers->initialized) {
    kl_int64_list_init(subscribers, (KL_ListConfig){
      .allocator = bus->config.allocator,
      .capacity = 8
    });
  }

  kl_int64_list_push(subscribers, sub_id);
  return sub_id;
}

int64_t kl_NAME_eventbus_sub(KL_NAME_EventBus* bus, T_X event_type, KL_NAME_EventBusListener listener) {
  return kl_NAME_eventbus_sub_(bus, event_type, false, listener);
}

int64_t kl_NAME_eventbus_sub_all(KL_NAME_EventBus* bus, KL_NAME_EventBusListener listener) {
  T_X event_type;
  memset(&event_type, 0, sizeof(T_X));
  return kl_NAME_eventbus_sub_(bus, event_type, true, listener);
}

void kl_NAME_eventbus_unsub(KL_NAME_EventBus* bus, int64_t sub_id) {
  assert(bus->initialized);

  KL_NAME_EventBusSubscriber* sub = (KL_NAME_EventBusSubscriber*)kl_binary_pool_get(&bus->subscriber_pool, sub_id);
  if (sub == 0) return;

  if (sub->is_global) {
    int64_t idx = kl_int64_list_index_of(&bus->global_subscribers, sub_id);
    if (idx >= 0) kl_int64_list_pop_at(&bus->global_subscribers, idx, 0);
  } else {
    T_X event_type = sub->event_type;
    KL_int64_List* subscribers = 0;
    if (kl_int64_list_hashmap_get_ptr(&bus->subscriber_lists, KL_BLOB_AUTO(event_type), &subscribers) && subscribers != 0 && subscribers->initialized) {
      int64_t idx = kl_int64_list_index_of(subscribers, sub_id);
      if (idx >= 0) kl_int64_list_pop_at(subscribers, idx, 0);
    }
  }

  kl_binary_pool_release(&bus->subscriber_pool, sub_id);
}

void kl_NAME_eventbus_unsub_deferred(KL_NAME_EventBus* bus, int64_t sub_id) {
  assert(bus->initialized);

  if (bus->emit_depth <= 0) {
    kl_NAME_eventbus_unsub(bus, sub_id);
    return;
  }

  if (!kl_int64_list_includes(&bus->pending_unsubs, sub_id)) {
    kl_int64_list_push(&bus->pending_unsubs, sub_id);
  }
}
