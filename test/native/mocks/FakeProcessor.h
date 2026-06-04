// FakeProcessor — a scriptable test double for the Processor interface.
//
// Drives SubscriptionManager under test: records publishes/subscribes and lets
// a test script the return values of maintain()/isConnected()/init()/ready() so
// reconnection decisions (loop() returning false => rebuild) can be asserted
// deterministically.
#pragma once

#include <deque>
#include <string>
#include <utility>
#include <vector>

#include "processors/Processor.h"

class FakeProcessor : public Processor {
 public:
  // --- scripted return values (a queue front is popped per call; when the
  //     queue is empty the corresponding default is returned) ---
  std::deque<bool> maintainScript;
  std::deque<bool> isConnectedScript;
  std::deque<bool> initScript;
  bool defaultMaintain = true;
  bool defaultConnected = true;
  bool defaultInit = true;
  bool defaultReady = true;
  bool defaultPublish = true;
  bool defaultSubscribe = true;

  // --- recorded interactions ---
  std::vector<std::pair<std::string, std::string>> publishes;  // (topic,payload)
  std::vector<std::string> subscribes;
  std::vector<std::string> unsubscribes;
  int loopCalls = 0;
  int maintainCalls = 0;
  int initCalls = 0;
  int connectCalls = 0;
  int disconnectCalls = 0;

  bool connect() override {
    connectCalls++;
    return true;
  }
  void disconnect() override { disconnectCalls++; }

  bool isConnected() override { return pop(isConnectedScript, defaultConnected); }

  bool init() override {
    initCalls++;
    return pop(initScript, defaultInit);
  }

  void loop() override { loopCalls++; }
  bool ready() override { return defaultReady; }

  bool maintain() override {
    maintainCalls++;
    return pop(maintainScript, defaultMaintain);
  }

  bool publish(const char* topic, const char* payload) override {
    publishes.emplace_back(topic ? topic : "", payload ? payload : "");
    return defaultPublish;
  }
  bool publish(const char* topic, uint8_t*, size_t) override {
    publishes.emplace_back(topic ? topic : "", "<binary>");
    return defaultPublish;
  }

  bool subscribe(const char* topic,
                 std::function<void(const char*, const char*)>) override {
    subscribes.emplace_back(topic ? topic : "");
    return defaultSubscribe;
  }
  bool unsubscribe(const char* topic) override {
    unsubscribes.emplace_back(topic ? topic : "");
    return true;
  }

 private:
  static bool pop(std::deque<bool>& q, bool dflt) {
    if (q.empty()) return dflt;
    bool v = q.front();
    q.pop_front();
    return v;
  }
};
