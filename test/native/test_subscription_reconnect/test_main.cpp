// Native tests for SubscriptionManager::loop() reconnection semantics — the
// contract that upstream (HyphenRunner) relies on: loop() returns false to
// signal "connection broken, rebuild". Time is driven deterministically via the
// fake millis() clock so keep-alive boundaries are exact.
#include <unity.h>

#include "managers/SubscriptionManager.h"
#include "mocks/FakeProcessor.h"
#include "test_clock.h"

// KEEP_ALIVE_INTERVAL defaults to 20s -> 20000ms.
static const unsigned long KA_MS = 20000;
static const char* kRegistrationTopic = "Hy/Post/Register/testdevice0001";

void setUp() { setMillis(0); }
void tearDown() {}

void test_loop_true_before_keepalive_interval() {
  FakeProcessor proc;
  SubscriptionManager mgr(proc);
  advanceMillis(KA_MS - 1);  // not yet due

  TEST_ASSERT_TRUE(mgr.loop());
  TEST_ASSERT_EQUAL_INT(0, proc.maintainCalls);  // health not probed yet
  TEST_ASSERT_EQUAL_INT(1, proc.loopCalls);      // but mqtt loop pumped
}

void test_loop_false_when_maintain_fails() {
  FakeProcessor proc;
  proc.maintainScript = {false};  // transport/MQTT health check fails
  SubscriptionManager mgr(proc);
  advanceMillis(KA_MS + 1);  // keep-alive due

  // loop() returns maintain()'s result => false => upstream triggers rebuild.
  TEST_ASSERT_FALSE(mgr.loop());
  TEST_ASSERT_EQUAL_INT(1, proc.maintainCalls);
}

void test_loop_true_when_maintain_succeeds() {
  FakeProcessor proc;
  proc.defaultMaintain = true;
  SubscriptionManager mgr(proc);
  advanceMillis(KA_MS + 1);

  TEST_ASSERT_TRUE(mgr.loop());
  TEST_ASSERT_EQUAL_INT(1, proc.maintainCalls);
}

// A dropped socket is reacted to within one loop() iteration — without waiting
// for the keep-alive interval to elapse.
void test_loop_reacts_immediately_to_dropped_socket() {
  FakeProcessor proc;
  proc.isConnectedScript = {false};  // socket dropped this iteration
  proc.maintainScript = {false};     // and recovery fails -> rebuild
  SubscriptionManager mgr(proc);
  // NOTE: clock NOT advanced — keep-alive interval has not elapsed.

  TEST_ASSERT_FALSE(mgr.loop());  // still reacts immediately
  TEST_ASSERT_EQUAL_INT(1, proc.maintainCalls);
}

// On reconnect HyphenRunner calls init(sendRegistration=false): topics must be
// re-subscribed, but the cloud catalog manifest is intentionally NOT re-sent.
void test_registration_not_resent_on_reconnect() {
  FakeProcessor proc;
  SubscriptionManager mgr(proc);

  TEST_ASSERT_TRUE(mgr.init(/*sendRegistration=*/false));
  mgr.test_setApplyRegistration(true);
  mgr.loop();  // runs runRegistration()

  TEST_ASSERT_TRUE(mgr.ready());  // subscriptionDone set without registering
  // function + variable topics were re-subscribed...
  TEST_ASSERT_EQUAL_size_t(2, proc.subscribes.size());
  // ...but no manifest was published.
  TEST_ASSERT_EQUAL_size_t(0, proc.publishes.size());
}

// First connect (sendRegistration=true) DOES publish the manifest once the
// registry is marked ready.
void test_registration_published_on_first_connect() {
  FakeProcessor proc;
  SubscriptionManager mgr(proc);
  mgr.function("doThing", [](const char*) { return 1; });

  TEST_ASSERT_TRUE(mgr.init(/*sendRegistration=*/true));
  mgr.test_setApplyRegistration(true);
  mgr.loop();  // runRegistration() -> registerFunctions() (no manifest yet)
  TEST_ASSERT_EQUAL_size_t(0, proc.publishes.size());

  mgr.test_setCheckReady(true);
  mgr.loop();  // registryReady() -> sendRegistry() publishes the manifest

  TEST_ASSERT_TRUE(mgr.ready());
  TEST_ASSERT_EQUAL_size_t(1, proc.publishes.size());
  TEST_ASSERT_EQUAL_STRING(kRegistrationTopic, proc.publishes[0].first.c_str());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_loop_true_before_keepalive_interval);
  RUN_TEST(test_loop_false_when_maintain_fails);
  RUN_TEST(test_loop_true_when_maintain_succeeds);
  RUN_TEST(test_loop_reacts_immediately_to_dropped_socket);
  RUN_TEST(test_registration_not_resent_on_reconnect);
  RUN_TEST(test_registration_published_on_first_connect);
  return UNITY_END();
}
