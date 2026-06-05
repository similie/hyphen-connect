// FakeSecureClient — no-op SecureClient so a FakeConnection can satisfy the
// secureClient()/getNewSecureClient() return types. Records which cert setters
// were called in case a test wants to assert TLS setup occurred.
#pragma once

#include "connections/Connection.h"

class FakeSecureClient : public SecureClient {
 public:
  int caCertCalls = 0, certCalls = 0, keyCalls = 0;
  bool insecure = false;

  // SecureClient interface
  void setCACert(const char*) override { caCertCalls++; }
  void setCertificate(const char*) override { certCalls++; }
  void setPrivateKey(const char*) override { keyCalls++; }
  void setPreSharedKey(const char*, const char*) override {}
  void setInsecure() override { insecure = true; }
  void setCACertBundle(const uint8_t*) override {}
  void setHandshakeTimeout(unsigned long) override {}
  bool verify(const char*, const char*) override { return true; }
  void setClient(Client*) override {}

  // Client interface
  int connect(IPAddress, uint16_t) override { return 1; }
  int connect(const char*, uint16_t) override { return 1; }
  size_t write(uint8_t) override { return 1; }
  size_t write(const uint8_t*, size_t n) override { return n; }
  int available() override { return 0; }
  int read() override { return -1; }
  int read(uint8_t*, size_t) override { return -1; }
  int peek() override { return -1; }
  void flush() override {}
  void stop() override {}
  uint8_t connected() override { return 0; }
  operator bool() override { return false; }
};
