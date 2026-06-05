// Native unit tests for SubscriptionManager's remote variable/function payload
// building and topic parsing. runVariable()/runFunction() are pure: they parse
// the MQTT topic (key + callId), look up the registry, and return the JSON
// response that gets published back to the cloud. We assert the JSON shape and,
// implicitly, the getTopicKey()/getCallId() parsing (which are private).
#include <unity.h>

#include <ArduinoJson.h>

#include "managers/SubscriptionManager.h"
#include "mocks/FakeProcessor.h"

// DEVICE_PUBLIC_ID is injected via build_flags for the native env.
static const char* kDeviceId = "testdevice0001";

// A request topic shaped like the broker delivers: ".../<key>/<callId>".
static const char* varTopic(const char* key) {
  static std::string buf;
  buf = std::string("Hy/Post/Variable/") + kDeviceId + "/" + key + "/req99";
  return buf.c_str();
}
static const char* fnTopic(const char* key) {
  static std::string buf;
  buf = std::string("Hy/Post/Function/") + kDeviceId + "/" + key + "/req99";
  return buf.c_str();
}

void setUp() {}
void tearDown() {}

void test_runVariable_int_payload_shape() {
  FakeProcessor proc;
  SubscriptionManager mgr(proc);
  int value = 4242;
  mgr.variable("ticker", &value);

  String out = mgr.runVariable(varTopic("ticker"), "");

  JsonDocument doc;
  DeserializationError err = deserializeJson(doc, out.c_str());
  TEST_ASSERT_FALSE(err);
  TEST_ASSERT_EQUAL_STRING("ticker", doc["key"].as<const char*>());
  TEST_ASSERT_EQUAL_STRING(kDeviceId, doc["id"].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("req99", doc["request"].as<const char*>());
  TEST_ASSERT_EQUAL_INT(4242, doc["value"].as<int>());
}

void test_runVariable_reflects_live_pointer_value() {
  FakeProcessor proc;
  SubscriptionManager mgr(proc);
  long value = 1;
  mgr.variable("count", &value);

  value = 777;  // mutate after registration
  String out = mgr.runVariable(varTopic("count"), "");

  JsonDocument doc;
  TEST_ASSERT_FALSE(deserializeJson(doc, out.c_str()));
  TEST_ASSERT_EQUAL_INT(777, doc["value"].as<long>());
}

void test_runVariable_string_type() {
  FakeProcessor proc;
  SubscriptionManager mgr(proc);
  String value = "hello-world";
  mgr.variable("label", &value);

  String out = mgr.runVariable(varTopic("label"), "");
  JsonDocument doc;
  TEST_ASSERT_FALSE(deserializeJson(doc, out.c_str()));
  TEST_ASSERT_EQUAL_STRING("hello-world", doc["value"].as<const char*>());
}

void test_runVariable_unknown_returns_empty() {
  FakeProcessor proc;
  SubscriptionManager mgr(proc);
  String out = mgr.runVariable(varTopic("does_not_exist"), "");
  TEST_ASSERT_TRUE(out.isEmpty());
}

void test_runFunction_invokes_callback_and_returns_value() {
  FakeProcessor proc;
  SubscriptionManager mgr(proc);
  int captured = -1;
  mgr.function("doThing", [&](const char* params) -> int {
    captured = params && params[0] ? (int)params[0] : 0;
    return 99;
  });

  String out = mgr.runFunction(fnTopic("doThing"), "Z");

  JsonDocument doc;
  TEST_ASSERT_FALSE(deserializeJson(doc, out.c_str()));
  TEST_ASSERT_EQUAL_STRING("doThing", doc["key"].as<const char*>());
  TEST_ASSERT_EQUAL_STRING(kDeviceId, doc["id"].as<const char*>());
  TEST_ASSERT_EQUAL_STRING("req99", doc["request"].as<const char*>());
  TEST_ASSERT_EQUAL_INT(99, doc["value"].as<int>());
  TEST_ASSERT_EQUAL_INT((int)'Z', captured);  // payload reached the callback
}

void test_runFunction_unknown_returns_empty() {
  FakeProcessor proc;
  SubscriptionManager mgr(proc);
  String out = mgr.runFunction(fnTopic("nope"), "");
  TEST_ASSERT_TRUE(out.isEmpty());
}

void test_buildRegistryPayload_manifest_shape() {
  FakeProcessor proc;
  SubscriptionManager mgr(proc);
  int a = 0;
  mgr.function("fnOne", [](const char*) { return 1; });
  mgr.function("fnTwo", [](const char*) { return 2; });
  mgr.variable("varOne", &a);

  String out = mgr.buildRegistryPayload();
  JsonDocument doc;
  TEST_ASSERT_FALSE(deserializeJson(doc, out.c_str()));
  TEST_ASSERT_EQUAL_STRING(kDeviceId, doc["id"].as<const char*>());
  TEST_ASSERT_EQUAL_INT(2, doc["functionCount"].as<int>());
  TEST_ASSERT_EQUAL_INT(1, doc["variableCount"].as<int>());
  TEST_ASSERT_EQUAL_size_t(2, doc["functions"].as<JsonArray>().size());
  TEST_ASSERT_EQUAL_size_t(1, doc["variables"].as<JsonArray>().size());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_runVariable_int_payload_shape);
  RUN_TEST(test_runVariable_reflects_live_pointer_value);
  RUN_TEST(test_runVariable_string_type);
  RUN_TEST(test_runVariable_unknown_returns_empty);
  RUN_TEST(test_runFunction_invokes_callback_and_returns_value);
  RUN_TEST(test_runFunction_unknown_returns_empty);
  RUN_TEST(test_buildRegistryPayload_manifest_shape);
  return UNITY_END();
}
