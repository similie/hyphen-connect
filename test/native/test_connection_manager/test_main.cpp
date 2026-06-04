// Native unit tests for ConnectionManager failover/recovery, driven by injected
// FakeConnections. These exercise the exact logic that decides whether a device
// stays on its current link, fails over to another, or reports the whole stack
// as down (which upstream turns into a rebuild) — the heart of the field issue.
#include <unity.h>

#include <memory>
#include <vector>

#include "connections/ConnectionManager.h"
#include "mocks/FakeConnection.h"

void setUp() {}
void tearDown() {}

static std::vector<std::unique_ptr<Connection>> makeList(
    std::unique_ptr<FakeConnection> a, std::unique_ptr<FakeConnection> b) {
  std::vector<std::unique_ptr<Connection>> v;
  v.push_back(std::move(a));
  if (b) v.push_back(std::move(b));
  return v;
}

void test_failover_skips_dead_connection_and_selects_next() {
  auto cell = std::make_unique<FakeConnection>(ConnectionClass::CELLULAR);
  auto wifi = std::make_unique<FakeConnection>(ConnectionClass::WIFI);
  FakeConnection* pCell = cell.get();
  FakeConnection* pWifi = wifi.get();

  cell->defaultInit = false;      // cellular never comes up
  wifi->defaultInit = true;       // wifi succeeds
  wifi->defaultConnected = true;

  ConnectionManager mgr(makeList(std::move(cell), std::move(wifi)),
                        ConnectionType::CELLULAR_PREFERRED);

  TEST_ASSERT_TRUE(mgr.init());
  TEST_ASSERT_TRUE(mgr.isConnected());
  TEST_ASSERT_EQUAL_INT((int)ConnectionClass::WIFI, (int)mgr.getClass());
  // failed cellular was powered off while iterating; selected wifi was not
  TEST_ASSERT_TRUE(pCell->offCalls >= 1);
  TEST_ASSERT_EQUAL_INT(0, pWifi->offCalls);
}

void test_all_connections_down_reports_disconnected() {
  auto cell = std::make_unique<FakeConnection>(ConnectionClass::CELLULAR);
  auto wifi = std::make_unique<FakeConnection>(ConnectionClass::WIFI);
  FakeConnection* pCell = cell.get();
  FakeConnection* pWifi = wifi.get();
  cell->defaultInit = false;
  wifi->defaultInit = false;

  ConnectionManager mgr(makeList(std::move(cell), std::move(wifi)),
                        ConnectionType::CELLULAR_PREFERRED);

  TEST_ASSERT_FALSE(mgr.init());
  TEST_ASSERT_FALSE(mgr.isConnected());
  // every candidate was attempted and powered off
  TEST_ASSERT_TRUE(pCell->offCalls >= 1);
  TEST_ASSERT_TRUE(pWifi->offCalls >= 1);
}

void test_maintain_false_when_no_current_connection() {
  auto cell = std::make_unique<FakeConnection>(ConnectionClass::CELLULAR);
  ConnectionManager mgr(makeList(std::move(cell), nullptr),
                        ConnectionType::CELLULAR_ONLY);
  // never init()'d -> no current connection
  TEST_ASSERT_FALSE(mgr.maintain());
}

void test_maintain_reflects_current_connection_health() {
  auto cell = std::make_unique<FakeConnection>(ConnectionClass::CELLULAR);
  FakeConnection* p = cell.get();
  cell->defaultInit = true;
  cell->defaultConnected = true;
  cell->maintainScript = {true, false};  // healthy, then the data path drops

  ConnectionManager mgr(makeList(std::move(cell), nullptr),
                        ConnectionType::CELLULAR_ONLY);
  TEST_ASSERT_TRUE(mgr.init());

  TEST_ASSERT_TRUE(mgr.maintain());   // first probe: alive
  TEST_ASSERT_FALSE(mgr.maintain());  // second probe: dead -> upstream rebuild
  TEST_ASSERT_EQUAL_INT(2, p->maintainCalls);
}

void test_client_accessors_safe_without_current_connection() {
  // A manager that was never init()'d (or was just disconnect()'d) has a null
  // currentConnection. The client accessors must not dereference it.
  auto cell = std::make_unique<FakeConnection>(ConnectionClass::CELLULAR);
  ConnectionManager mgr(makeList(std::move(cell), nullptr),
                        ConnectionType::CELLULAR_ONLY);

  Client& c = mgr.getClient();
  Client& nc = mgr.getNewClient();
  SecureClient& sc = mgr.secureClient();
  SecureClient& nsc = mgr.getNewSecureClient();

  // All return inert null-objects that report "not connected".
  TEST_ASSERT_FALSE((bool)c);
  TEST_ASSERT_FALSE((bool)nc);
  TEST_ASSERT_FALSE((bool)sc);
  TEST_ASSERT_FALSE((bool)nsc);
}

void test_single_connection_sets_class() {
  auto cell = std::make_unique<FakeConnection>(ConnectionClass::CELLULAR);
  cell->defaultInit = true;
  cell->defaultConnected = true;
  ConnectionManager mgr(makeList(std::move(cell), nullptr),
                        ConnectionType::CELLULAR_ONLY);
  TEST_ASSERT_TRUE(mgr.init());
  TEST_ASSERT_EQUAL_INT((int)ConnectionClass::CELLULAR, (int)mgr.getClass());
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_failover_skips_dead_connection_and_selects_next);
  RUN_TEST(test_all_connections_down_reports_disconnected);
  RUN_TEST(test_maintain_false_when_no_current_connection);
  RUN_TEST(test_maintain_reflects_current_connection_health);
  RUN_TEST(test_client_accessors_safe_without_current_connection);
  RUN_TEST(test_single_connection_sets_class);
  return UNITY_END();
}
