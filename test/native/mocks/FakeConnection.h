// FakeConnection — a scriptable Connection used to drive ConnectionManager
// failover/recovery under test without any radio hardware.
//
// Scripted return queues (front popped per call; falls back to the default when
// empty) let a test express drop/recover sequences, and `events` records the
// ordered lifecycle calls so failover ordering can be asserted.
#pragma once

#include <ctime>
#include <deque>
#include <string>
#include <vector>

#include "connections/Connection.h"
#include "mocks/FakeSecureClient.h"

class FakeConnection : public Connection {
 public:
  explicit FakeConnection(ConnectionClass klass) : klass_(klass) {}

  // scripted behavior
  std::deque<bool> initScript;
  std::deque<bool> isConnectedScript;
  std::deque<bool> maintainScript;
  bool defaultInit = true;
  bool defaultConnected = true;
  bool defaultMaintain = true;

  // recorded interactions
  std::vector<std::string> events;
  int initCalls = 0, connectCalls = 0, disconnectCalls = 0;
  int onCalls = 0, offCalls = 0, maintainCalls = 0;

  bool init() override {
    initCalls++;
    bool r = pop(initScript, defaultInit);
    events.emplace_back(r ? "init:true" : "init:false");
    return r;
  }
  bool connect() override {
    connectCalls++;
    events.emplace_back("connect");
    return true;
  }
  void disconnect() override {
    disconnectCalls++;
    events.emplace_back("disconnect");
  }
  bool isConnected() override { return pop(isConnectedScript, defaultConnected); }
  bool on() override {
    onCalls++;
    events.emplace_back("on");
    return true;
  }
  bool off() override {
    offCalls++;
    events.emplace_back("off");
    return true;
  }
  bool keepAlive(uint8_t) override { return true; }
  bool maintain() override {
    maintainCalls++;
    return pop(maintainScript, defaultMaintain);
  }
  void restore() override {}
  bool powerSave(bool) override { return true; }
  bool getTime(struct tm&, float&) override { return false; }
  ConnectionClass getClass() override { return klass_; }
  Connection& connection() override { return *this; }

  Client& getClient() override { return sec_; }
  SecureClient& secureClient() override { return sec_; }
  Client& getNewClient() override { return sec_; }
  SecureClient& getNewSecureClient() override { return sec_; }

 private:
  ConnectionClass klass_;
  FakeSecureClient sec_;

  static bool pop(std::deque<bool>& q, bool dflt) {
    if (q.empty()) return dflt;
    bool v = q.front();
    q.pop_front();
    return v;
  }
};
