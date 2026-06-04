// Native unit tests for CommunicationRegistry — the singleton that maps MQTT
// topics to callbacks and performs (prefix-style) wildcard matching. These lock
// down the public dispatch behavior that SecureMQTTProcessor relies on, in
// particular iterateCallbacks() (used to re-subscribe every topic on reconnect).
#include <unity.h>

#include <string>
#include <vector>

#include "managers/CommunicationRegistry.h"

// The registry is a process-wide singleton; drain it before/after each test so
// suites are independent.
static void resetRegistry() {
  auto& reg = CommunicationRegistry::getInstance();
  while (reg.getCallbackCount() > 0) {
    std::string first = reg.getCallbacks()[0];
    if (!reg.unregisterCallback(first)) break;  // guard against a stuck entry
  }
}

void setUp() { resetRegistry(); }
void tearDown() { resetRegistry(); }

static std::function<void(const char*, const char*)> noopCb() {
  return [](const char*, const char*) {};
}

void test_exact_match_registers_and_triggers() {
  auto& reg = CommunicationRegistry::getInstance();
  int hits = 0;
  std::string seenPayload;
  reg.registerCallback("Hy/Post/Config",
                       [&](const char*, const char* payload) {
                         hits++;
                         seenPayload = payload;
                       });

  TEST_ASSERT_TRUE(reg.hasCallback("Hy/Post/Config"));
  TEST_ASSERT_EQUAL_size_t(1, reg.getCallbackCount());

  reg.triggerCallbacks("Hy/Post/Config", "hello");
  TEST_ASSERT_EQUAL_INT(1, hits);
  TEST_ASSERT_EQUAL_STRING("hello", seenPayload.c_str());
}

void test_hash_wildcard_matches_prefix() {
  auto& reg = CommunicationRegistry::getInstance();
  // This is exactly how SubscriptionManager subscribes to function topics.
  reg.registerCallback("Hy/Post/Function/testdevice0001/#", noopCb());

  // A concrete function call topic shares the prefix => matches.
  TEST_ASSERT_TRUE(
      reg.hasCallback("Hy/Post/Function/testdevice0001/myFunc/req42"));
  // A different branch does not.
  TEST_ASSERT_FALSE(reg.hasCallback("Hy/Post/Variable/testdevice0001/x/req1"));
}

void test_no_match_does_not_trigger() {
  auto& reg = CommunicationRegistry::getInstance();
  int hits = 0;
  reg.registerCallback("a/b/c", [&](const char*, const char*) { hits++; });

  TEST_ASSERT_FALSE(reg.hasCallback("x/y/z"));
  reg.triggerCallbacks("x/y/z", "ignored");
  TEST_ASSERT_EQUAL_INT(0, hits);
}

void test_duplicate_registration_keeps_single_entry() {
  auto& reg = CommunicationRegistry::getInstance();
  reg.registerCallback("dup/topic", noopCb());
  reg.registerCallback("dup/topic", noopCb());
  TEST_ASSERT_EQUAL_size_t(1, reg.getCallbackCount());
}

void test_unregister_removes_and_decrements() {
  auto& reg = CommunicationRegistry::getInstance();
  reg.registerCallback("aaa", noopCb());
  reg.registerCallback("bbb", noopCb());
  reg.registerCallback("ccc", noopCb());
  TEST_ASSERT_EQUAL_size_t(3, reg.getCallbackCount());

  TEST_ASSERT_TRUE(reg.unregisterCallback("bbb"));
  TEST_ASSERT_EQUAL_size_t(2, reg.getCallbackCount());
  TEST_ASSERT_FALSE(reg.hasCallback("bbb"));
  TEST_ASSERT_TRUE(reg.hasCallback("aaa"));
  TEST_ASSERT_TRUE(reg.hasCallback("ccc"));
}

void test_unregister_unknown_returns_false() {
  auto& reg = CommunicationRegistry::getInstance();
  TEST_ASSERT_FALSE(reg.unregisterCallback("never/registered"));
}

void test_iterate_visits_every_registered_topic() {
  auto& reg = CommunicationRegistry::getInstance();
  reg.registerCallback("t/one", noopCb());
  reg.registerCallback("t/two", noopCb());
  reg.registerCallback("t/three", noopCb());

  std::vector<std::string> visited;
  reg.iterateCallbacks(
      [&](const char* topic) { visited.emplace_back(topic); });

  TEST_ASSERT_EQUAL_size_t(3, visited.size());
  // order-independent membership check
  bool one = false, two = false, three = false;
  for (auto& t : visited) {
    one |= (t == "t/one");
    two |= (t == "t/two");
    three |= (t == "t/three");
  }
  TEST_ASSERT_TRUE(one && two && three);
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_exact_match_registers_and_triggers);
  RUN_TEST(test_hash_wildcard_matches_prefix);
  RUN_TEST(test_no_match_does_not_trigger);
  RUN_TEST(test_duplicate_registration_keeps_single_entry);
  RUN_TEST(test_unregister_removes_and_decrements);
  RUN_TEST(test_unregister_unknown_returns_false);
  RUN_TEST(test_iterate_visits_every_registered_topic);
  return UNITY_END();
}
