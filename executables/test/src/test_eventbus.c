#include <test/tests.h>

// ---------- eventbus ----------

typedef enum {
  KL_TEST_EVENT_CLICK,
  KL_TEST_EVENT_MESSAGE,
  KL_TEST_EVENT_EMAIL
} KL_TestEventType;

typedef union {
  struct { float x; float y; } click;
  struct { const char* value; } message;
  struct { const char* address; } email;
} KL_TestEventPayload;

KL_DEFINE_EVENTBUS(testev, KL_TestEventType, KL_TestEventPayload)
KL_IMPL_EVENTBUS(testev, KL_TestEventType, KL_TestEventPayload)

typedef struct {
  KL_Allocator a;
  KL_testev_EventBus bus;
} KL_TestEB;

static void kl_test_eb_setup(KL_TestEB* fx, int64_t listener_capacity) {
  kl_allocator_init(&fx->a, (KL_AllocatorConfig){
    .capacity = 256 * 1024,
    .track_allocations = false,
  });
  fx->bus = (KL_testev_EventBus){0};
  kl_testev_eventbus_init(&fx->bus, (KL_EventBusConfig){
    .listener_capacity = listener_capacity,
    .allocator = &fx->a,
  });
}

static void kl_test_eb_teardown(KL_TestEB* fx) {
  kl_testev_eventbus_destroy(&fx->bus);
  kl_allocator_destroy(&fx->a);
}

typedef struct {
  int64_t calls;
  KL_testev_EventBusEvent last_event;
  KL_EventBusSignal signal;
} KL_TestEBRecorder;

static KL_EventBusSignal kl_test_eb_record(void* user_ptr, const KL_testev_EventBusEvent* event) {
  KL_TestEBRecorder* rec = (KL_TestEBRecorder*)user_ptr;
  rec->calls += 1;
  rec->last_event = *event;
  return rec->signal;
}

void test_eventbus_init_destroy() {
  KL_TestEB fx = {0};
  kl_test_eb_setup(&fx, 8);

  KL_TEST_ASSERT(fx.bus.initialized == true, "initialized after init");
  KL_TEST_ASSERT(fx.bus.subscriber_lists.initialized == true, "subscriber_lists initialized");
  KL_TEST_ASSERT(fx.bus.global_subscribers.initialized == true, "global_subscribers initialized");
  KL_TEST_ASSERT(fx.bus.subscriber_pool.initialized == true, "subscriber_pool initialized");

  kl_testev_eventbus_destroy(&fx.bus);
  KL_TEST_ASSERT(fx.bus.initialized == false, "initialized cleared after destroy");

  kl_allocator_destroy(&fx.a);
}

void test_eventbus_double_init_is_noop() {
  KL_TestEB fx = {0};
  kl_test_eb_setup(&fx, 8);

  kl_testev_eventbus_init(&fx.bus, (KL_EventBusConfig){
    .listener_capacity = 9999,
    .allocator = &fx.a,
  });
  KL_TEST_EXPECT_EQ_INT((int)fx.bus.config.listener_capacity, 8);

  kl_test_eb_teardown(&fx);
}

void test_eventbus_emit_without_subscribers_is_safe() {
  KL_TestEB fx = {0};
  kl_test_eb_setup(&fx, 8);

  KL_testev_EventBusEvent ev = { .type = KL_TEST_EVENT_CLICK };
  kl_testev_eventbus_emit(&fx.bus, &ev);

  kl_test_eb_teardown(&fx);
}

void test_eventbus_sub_emit_dispatches_payload() {
  KL_TestEB fx = {0};
  kl_test_eb_setup(&fx, 8);

  KL_TestEBRecorder rec = {0};
  int64_t id = kl_testev_eventbus_sub(&fx.bus, KL_TEST_EVENT_CLICK, (KL_testev_EventBusListener){
    .fn = kl_test_eb_record,
    .user_ptr = &rec,
  });
  KL_TEST_ASSERT(id >= 0, "sub returns a valid id");

  KL_testev_EventBusEvent ev = {
    .type = KL_TEST_EVENT_CLICK,
    .payload.click = { .x = 3.3f, .y = 5.5f },
  };
  kl_testev_eventbus_emit(&fx.bus, &ev);

  KL_TEST_EXPECT_EQ_INT((int)rec.calls, 1);
  KL_TEST_EXPECT_EQ_INT((int)rec.last_event.type, (int)KL_TEST_EVENT_CLICK);
  KL_TEST_ASSERT(rec.last_event.payload.click.x == 3.3f, "payload x round-trips");
  KL_TEST_ASSERT(rec.last_event.payload.click.y == 5.5f, "payload y round-trips");

  kl_test_eb_teardown(&fx);
}

void test_eventbus_emit_only_reaches_matching_type() {
  KL_TestEB fx = {0};
  kl_test_eb_setup(&fx, 8);

  KL_TestEBRecorder click_rec = {0};
  KL_TestEBRecorder mail_rec = {0};
  kl_testev_eventbus_sub(&fx.bus, KL_TEST_EVENT_CLICK, (KL_testev_EventBusListener){
    .fn = kl_test_eb_record, .user_ptr = &click_rec });
  kl_testev_eventbus_sub(&fx.bus, KL_TEST_EVENT_EMAIL, (KL_testev_EventBusListener){
    .fn = kl_test_eb_record, .user_ptr = &mail_rec });

  KL_testev_EventBusEvent ev = {
    .type = KL_TEST_EVENT_EMAIL,
    .payload.email = { .address = "kl@example.com" },
  };
  kl_testev_eventbus_emit(&fx.bus, &ev);

  KL_TEST_EXPECT_EQ_INT((int)click_rec.calls, 0);
  KL_TEST_EXPECT_EQ_INT((int)mail_rec.calls, 1);
  KL_TEST_ASSERT(strcmp(mail_rec.last_event.payload.email.address, "kl@example.com") == 0,
                 "email payload round-trips");

  kl_test_eb_teardown(&fx);
}

void test_eventbus_multiple_listeners_all_called() {
  KL_TestEB fx = {0};
  kl_test_eb_setup(&fx, 8);

  KL_TestEBRecorder recs[3] = {0};
  for (int i = 0; i < 3; i++) {
    int64_t id = kl_testev_eventbus_sub(&fx.bus, KL_TEST_EVENT_MESSAGE, (KL_testev_EventBusListener){
      .fn = kl_test_eb_record, .user_ptr = &recs[i] });
    KL_TEST_ASSERT(id >= 0, "sub succeeds");
  }

  KL_testev_EventBusEvent ev = { .type = KL_TEST_EVENT_MESSAGE };
  kl_testev_eventbus_emit(&fx.bus, &ev);
  kl_testev_eventbus_emit(&fx.bus, &ev);

  for (int i = 0; i < 3; i++) {
    KL_TEST_EXPECT_EQ_INT((int)recs[i].calls, 2);
  }

  kl_test_eb_teardown(&fx);
}

void test_eventbus_sub_all_receives_every_type() {
  KL_TestEB fx = {0};
  kl_test_eb_setup(&fx, 8);

  KL_TestEBRecorder rec = {0};
  int64_t id = kl_testev_eventbus_sub_all(&fx.bus, (KL_testev_EventBusListener){
    .fn = kl_test_eb_record, .user_ptr = &rec });
  KL_TEST_ASSERT(id >= 0, "sub_all returns a valid id");

  KL_testev_EventBusEvent click = { .type = KL_TEST_EVENT_CLICK };
  KL_testev_EventBusEvent message = { .type = KL_TEST_EVENT_MESSAGE };
  KL_testev_EventBusEvent email = { .type = KL_TEST_EVENT_EMAIL };
  kl_testev_eventbus_emit(&fx.bus, &click);
  kl_testev_eventbus_emit(&fx.bus, &message);
  kl_testev_eventbus_emit(&fx.bus, &email);

  KL_TEST_EXPECT_EQ_INT((int)rec.calls, 3);
  KL_TEST_EXPECT_EQ_INT((int)rec.last_event.type, (int)KL_TEST_EVENT_EMAIL);

  kl_test_eb_teardown(&fx);
}

void test_eventbus_stop_signal_halts_dispatch() {
  KL_TestEB fx = {0};
  kl_test_eb_setup(&fx, 8);

  KL_TestEBRecorder first = { .signal = KL_EVENTBUS_SIGNAL_STOP };
  KL_TestEBRecorder second = {0};
  KL_TestEBRecorder global = {0};
  kl_testev_eventbus_sub(&fx.bus, KL_TEST_EVENT_CLICK, (KL_testev_EventBusListener){
    .fn = kl_test_eb_record, .user_ptr = &first });
  kl_testev_eventbus_sub(&fx.bus, KL_TEST_EVENT_CLICK, (KL_testev_EventBusListener){
    .fn = kl_test_eb_record, .user_ptr = &second });
  kl_testev_eventbus_sub_all(&fx.bus, (KL_testev_EventBusListener){
    .fn = kl_test_eb_record, .user_ptr = &global });

  KL_testev_EventBusEvent ev = { .type = KL_TEST_EVENT_CLICK };
  kl_testev_eventbus_emit(&fx.bus, &ev);

  KL_TEST_EXPECT_EQ_INT((int)first.calls, 1);
  KL_TEST_EXPECT_EQ_INT((int)second.calls, 0);
  KL_TEST_EXPECT_EQ_INT((int)global.calls, 0);

  kl_test_eb_teardown(&fx);
}

void test_eventbus_unsub_stops_delivery() {
  KL_TestEB fx = {0};
  kl_test_eb_setup(&fx, 8);

  KL_TestEBRecorder keep = {0};
  KL_TestEBRecorder drop = {0};
  kl_testev_eventbus_sub(&fx.bus, KL_TEST_EVENT_CLICK, (KL_testev_EventBusListener){
    .fn = kl_test_eb_record, .user_ptr = &keep });
  int64_t drop_id = kl_testev_eventbus_sub(&fx.bus, KL_TEST_EVENT_CLICK, (KL_testev_EventBusListener){
    .fn = kl_test_eb_record, .user_ptr = &drop });

  KL_testev_EventBusEvent ev = { .type = KL_TEST_EVENT_CLICK };
  kl_testev_eventbus_emit(&fx.bus, &ev);
  KL_TEST_EXPECT_EQ_INT((int)drop.calls, 1);

  kl_testev_eventbus_unsub(&fx.bus, drop_id);
  kl_testev_eventbus_emit(&fx.bus, &ev);

  KL_TEST_EXPECT_EQ_INT((int)keep.calls, 2);
  KL_TEST_EXPECT_EQ_INT((int)drop.calls, 1);

  kl_test_eb_teardown(&fx);
}

void test_eventbus_unsub_global_stops_delivery() {
  KL_TestEB fx = {0};
  kl_test_eb_setup(&fx, 8);

  KL_TestEBRecorder rec = {0};
  int64_t id = kl_testev_eventbus_sub_all(&fx.bus, (KL_testev_EventBusListener){
    .fn = kl_test_eb_record, .user_ptr = &rec });

  KL_testev_EventBusEvent ev = { .type = KL_TEST_EVENT_MESSAGE };
  kl_testev_eventbus_emit(&fx.bus, &ev);
  KL_TEST_EXPECT_EQ_INT((int)rec.calls, 1);

  kl_testev_eventbus_unsub(&fx.bus, id);
  kl_testev_eventbus_emit(&fx.bus, &ev);
  KL_TEST_EXPECT_EQ_INT((int)rec.calls, 1);
  KL_TEST_EXPECT_EQ_INT((int)fx.bus.global_subscribers.length, 0);

  kl_test_eb_teardown(&fx);
}

void test_eventbus_unsub_stale_id_is_noop() {
  KL_TestEB fx = {0};
  kl_test_eb_setup(&fx, 8);

  KL_TestEBRecorder rec = {0};
  int64_t id = kl_testev_eventbus_sub(&fx.bus, KL_TEST_EVENT_CLICK, (KL_testev_EventBusListener){
    .fn = kl_test_eb_record, .user_ptr = &rec });

  kl_testev_eventbus_unsub(&fx.bus, id);
  // second unsub with the now-stale id must not disturb anything
  kl_testev_eventbus_unsub(&fx.bus, id);
  kl_testev_eventbus_unsub(&fx.bus, -1);

  KL_testev_EventBusEvent ev = { .type = KL_TEST_EVENT_CLICK };
  kl_testev_eventbus_emit(&fx.bus, &ev);
  KL_TEST_EXPECT_EQ_INT((int)rec.calls, 0);

  kl_test_eb_teardown(&fx);
}

void test_eventbus_sub_exhausted_returns_negative() {
  KL_TestEB fx = {0};
  kl_test_eb_setup(&fx, 2);

  KL_TestEBRecorder rec = {0};
  KL_testev_EventBusListener listener = { .fn = kl_test_eb_record, .user_ptr = &rec };

  KL_TEST_ASSERT(kl_testev_eventbus_sub(&fx.bus, KL_TEST_EVENT_CLICK, listener) >= 0, "sub 1");
  KL_TEST_ASSERT(kl_testev_eventbus_sub(&fx.bus, KL_TEST_EVENT_CLICK, listener) >= 0, "sub 2");
  KL_TEST_ASSERT(kl_testev_eventbus_sub(&fx.bus, KL_TEST_EVENT_CLICK, listener) == -1,
                 "sub on exhausted pool returns -1");

  kl_test_eb_teardown(&fx);
}

typedef struct {
  KL_testev_EventBus* bus;
  int64_t own_id;
  int64_t calls;
} KL_TestEBSelfUnsub;

static KL_EventBusSignal kl_test_eb_self_unsub(void* user_ptr, const KL_testev_EventBusEvent* event) {
  (void)event;
  KL_TestEBSelfUnsub* self = (KL_TestEBSelfUnsub*)user_ptr;
  self->calls += 1;
  kl_testev_eventbus_unsub_deferred(self->bus, self->own_id);
  return KL_EVENTBUS_SIGNAL_NONE;
}

void test_eventbus_unsub_deferred_inside_callback() {
  KL_TestEB fx = {0};
  kl_test_eb_setup(&fx, 8);

  KL_TestEBSelfUnsub self = { .bus = &fx.bus };
  KL_TestEBRecorder after = {0};
  self.own_id = kl_testev_eventbus_sub(&fx.bus, KL_TEST_EVENT_CLICK, (KL_testev_EventBusListener){
    .fn = kl_test_eb_self_unsub, .user_ptr = &self });
  KL_TEST_ASSERT(self.own_id >= 0, "sub succeeds");
  // registered after the self-unsubbing listener in the same list; must still
  // be reached during the emit that queues the unsub
  kl_testev_eventbus_sub(&fx.bus, KL_TEST_EVENT_CLICK, (KL_testev_EventBusListener){
    .fn = kl_test_eb_record, .user_ptr = &after });

  KL_testev_EventBusEvent ev = { .type = KL_TEST_EVENT_CLICK };
  kl_testev_eventbus_emit(&fx.bus, &ev);
  KL_TEST_EXPECT_EQ_INT((int)self.calls, 1);
  KL_TEST_EXPECT_EQ_INT((int)after.calls, 1);

  // the queued unsub was applied after the emit returned
  kl_testev_eventbus_emit(&fx.bus, &ev);
  KL_TEST_EXPECT_EQ_INT((int)self.calls, 1);
  KL_TEST_EXPECT_EQ_INT((int)after.calls, 2);
  KL_TEST_EXPECT_EQ_INT((int)fx.bus.pending_unsubs.length, 0);

  kl_test_eb_teardown(&fx);
}

void test_eventbus_unsub_deferred_outside_emit_is_immediate() {
  KL_TestEB fx = {0};
  kl_test_eb_setup(&fx, 8);

  KL_TestEBRecorder rec = {0};
  int64_t id = kl_testev_eventbus_sub(&fx.bus, KL_TEST_EVENT_CLICK, (KL_testev_EventBusListener){
    .fn = kl_test_eb_record, .user_ptr = &rec });

  kl_testev_eventbus_unsub_deferred(&fx.bus, id);
  KL_TEST_EXPECT_EQ_INT((int)fx.bus.pending_unsubs.length, 0);

  KL_testev_EventBusEvent ev = { .type = KL_TEST_EVENT_CLICK };
  kl_testev_eventbus_emit(&fx.bus, &ev);
  KL_TEST_EXPECT_EQ_INT((int)rec.calls, 0);

  kl_test_eb_teardown(&fx);
}

static KL_EventBusSignal kl_test_eb_self_unsub_twice(void* user_ptr, const KL_testev_EventBusEvent* event) {
  (void)event;
  KL_TestEBSelfUnsub* self = (KL_TestEBSelfUnsub*)user_ptr;
  self->calls += 1;
  kl_testev_eventbus_unsub_deferred(self->bus, self->own_id);
  kl_testev_eventbus_unsub_deferred(self->bus, self->own_id);
  return KL_EVENTBUS_SIGNAL_NONE;
}

void test_eventbus_unsub_deferred_double_queue_is_noop() {
  KL_TestEB fx = {0};
  kl_test_eb_setup(&fx, 8);

  KL_TestEBSelfUnsub self = { .bus = &fx.bus };
  self.own_id = kl_testev_eventbus_sub(&fx.bus, KL_TEST_EVENT_CLICK, (KL_testev_EventBusListener){
    .fn = kl_test_eb_self_unsub_twice, .user_ptr = &self });

  KL_testev_EventBusEvent ev = { .type = KL_TEST_EVENT_CLICK };
  kl_testev_eventbus_emit(&fx.bus, &ev);
  KL_TEST_EXPECT_EQ_INT((int)self.calls, 1);
  KL_TEST_EXPECT_EQ_INT((int)fx.bus.pending_unsubs.length, 0);

  kl_testev_eventbus_emit(&fx.bus, &ev);
  KL_TEST_EXPECT_EQ_INT((int)self.calls, 1);

  kl_test_eb_teardown(&fx);
}

void test_eventbus_unsub_then_resub_reuses_slot() {
  KL_TestEB fx = {0};
  kl_test_eb_setup(&fx, 1);

  KL_TestEBRecorder rec = {0};
  KL_testev_EventBusListener listener = { .fn = kl_test_eb_record, .user_ptr = &rec };

  int64_t id0 = kl_testev_eventbus_sub(&fx.bus, KL_TEST_EVENT_CLICK, listener);
  KL_TEST_ASSERT(id0 >= 0, "first sub succeeds");
  kl_testev_eventbus_unsub(&fx.bus, id0);

  int64_t id1 = kl_testev_eventbus_sub(&fx.bus, KL_TEST_EVENT_MESSAGE, listener);
  KL_TEST_ASSERT(id1 >= 0, "resub after unsub succeeds");
  KL_TEST_ASSERT(id1 != id0, "recycled slot yields a distinct id");

  KL_testev_EventBusEvent click = { .type = KL_TEST_EVENT_CLICK };
  KL_testev_EventBusEvent message = { .type = KL_TEST_EVENT_MESSAGE };
  kl_testev_eventbus_emit(&fx.bus, &click);
  KL_TEST_EXPECT_EQ_INT((int)rec.calls, 0);
  kl_testev_eventbus_emit(&fx.bus, &message);
  KL_TEST_EXPECT_EQ_INT((int)rec.calls, 1);

  kl_test_eb_teardown(&fx);
}
