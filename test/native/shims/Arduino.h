// Arduino.h — minimal native (host) shim for the subset of the Arduino core that
// HyphenConnect's pure-logic / manager translation units actually use.
//
// This file is ONLY on the include path for the [env:native] PlatformIO
// environment. The real ESP32 firmware build (env:esp32dev) uses the genuine
// Arduino core and never sees this header.
//
// It provides: a std::string-backed `String` with the Arduino String API that
// ArduinoJson can also serialize into, a controllable millis(), no-op Serial,
// the F() macro, and a minimal Client/Stream/IPAddress hierarchy so the
// Connection/SecureClient interfaces compile.
#pragma once

#include <cstdint>
#include <cstddef>
#include <cstring>
#include <cstdlib>
#include <string>
#include <sys/types.h>  // u_int8_t / u_int16_t used by production headers

#include "test_clock.h"

#ifndef F
#define F(string_literal) (string_literal)
#endif

typedef uint8_t byte;

inline unsigned long millis() {
  return hyphen_test_clock::g_millis;
}
inline unsigned long micros() {
  return hyphen_test_clock::g_millis * 1000UL;
}
inline void delay(unsigned long) {}
inline void delayMicroseconds(unsigned int) {}
inline void yield() {}

// ---------------------------------------------------------------------------
// String — std::string-backed, Arduino-compatible surface.
// Implements exactly the methods exercised by the natively-compiled TUs plus
// the concat()/operator=(const char*) that ArduinoJson's Writer<::String> needs.
// ---------------------------------------------------------------------------
class String {
 public:
  String() = default;
  String(const char* s) : _s(s ? s : "") {}
  String(const std::string& s) : _s(s) {}
  String(const String&) = default;
  explicit String(char c) : _s(1, c) {}
  explicit String(int v) : _s(std::to_string(v)) {}
  explicit String(unsigned int v) : _s(std::to_string(v)) {}
  explicit String(long v) : _s(std::to_string(v)) {}
  explicit String(unsigned long v) : _s(std::to_string(v)) {}
  explicit String(double v) : _s(std::to_string(v)) {}
  explicit String(float v) : _s(std::to_string(v)) {}
  explicit String(bool v) : _s(v ? "1" : "0") {}

  String& operator=(const String&) = default;
  String& operator=(const char* s) {
    _s = s ? s : "";
    return *this;
  }

  const char* c_str() const { return _s.c_str(); }
  size_t length() const { return _s.length(); }
  bool isEmpty() const { return _s.empty(); }

  // Concatenation (Arduino returns 1 on success; used by ArduinoJson writer).
  unsigned char concat(const char* s) {
    if (s) _s += s;
    return 1;
  }
  unsigned char concat(char c) {
    _s += c;
    return 1;
  }
  unsigned char concat(const String& o) {
    _s += o._s;
    return 1;
  }

  String& operator+=(const String& o) {
    _s += o._s;
    return *this;
  }
  String& operator+=(const char* s) {
    if (s) _s += s;
    return *this;
  }
  String& operator+=(char c) {
    _s += c;
    return *this;
  }

  bool operator==(const String& o) const { return _s == o._s; }
  bool operator==(const char* s) const { return _s == (s ? s : ""); }
  bool operator!=(const String& o) const { return _s != o._s; }
  bool operator!=(const char* s) const { return _s != (s ? s : ""); }

  int indexOf(char c) const {
    auto p = _s.find(c);
    return p == std::string::npos ? -1 : static_cast<int>(p);
  }
  int indexOf(const char* s) const {
    auto p = _s.find(s ? s : "");
    return p == std::string::npos ? -1 : static_cast<int>(p);
  }
  int indexOf(const String& o) const { return indexOf(o._s.c_str()); }
  int indexOf(char c, int from) const {
    if (from < 0) from = 0;
    auto p = _s.find(c, static_cast<size_t>(from));
    return p == std::string::npos ? -1 : static_cast<int>(p);
  }

  int lastIndexOf(char c) const {
    auto p = _s.rfind(c);
    return p == std::string::npos ? -1 : static_cast<int>(p);
  }
  int lastIndexOf(const char* s) const {
    auto p = _s.rfind(s ? s : "");
    return p == std::string::npos ? -1 : static_cast<int>(p);
  }

  // Arduino substring(from): from clamped to [0, len].
  String substring(int from) const {
    return substring(from, static_cast<int>(_s.length()));
  }
  // Arduino substring(from, to): half-open [from, to). Mirrors Arduino's
  // unsigned clamping so substring(0, -1) yields the whole string.
  String substring(int from, int to) const {
    int len = static_cast<int>(_s.length());
    if (to < 0 || to > len) to = len;
    if (from < 0) from = 0;
    if (from > len) from = len;
    if (from >= to) return String();
    return String(_s.substr(static_cast<size_t>(from),
                            static_cast<size_t>(to - from)));
  }

  bool startsWith(const String& prefix) const {
    return _s.size() >= prefix._s.size() &&
           _s.compare(0, prefix._s.size(), prefix._s) == 0;
  }
  bool startsWith(const char* prefix) const {
    return startsWith(String(prefix));
  }

  const std::string& std_str() const { return _s; }  // test convenience

 private:
  std::string _s;
};

inline String operator+(const String& a, const String& b) {
  String r(a);
  r += b;
  return r;
}
inline String operator+(const String& a, const char* b) {
  String r(a);
  r += b;
  return r;
}
inline String operator+(const char* a, const String& b) {
  String r(a);
  r += b;
  return r;
}
inline String operator+(const String& a, char b) {
  String r(a);
  r += b;
  return r;
}
inline bool operator==(const char* a, const String& b) { return b == a; }

// ---------------------------------------------------------------------------
// Serial / Print — no-op sinks.
// ---------------------------------------------------------------------------
struct SerialShim {
  void begin(unsigned long) {}
  void begin(unsigned long, int, int, int) {}
  void printf(const char*, ...) {}
  template <typename T> void print(const T&) {}
  template <typename T> void println(const T&) {}
  void println() {}
  size_t write(uint8_t) { return 1; }
  size_t write(const uint8_t*, size_t n) { return n; }
  void flush() {}
};
inline SerialShim Serial;

// ---------------------------------------------------------------------------
// Minimal Client/Stream/IPAddress so Connection.h + SecureClient compile.
// ---------------------------------------------------------------------------
class IPAddress {
 public:
  IPAddress() = default;
  IPAddress(uint8_t, uint8_t, uint8_t, uint8_t) {}
};

class Client {
 public:
  virtual ~Client() {}
  virtual int connect(IPAddress ip, uint16_t port) = 0;
  virtual int connect(const char* host, uint16_t port) = 0;
  virtual size_t write(uint8_t) = 0;
  virtual size_t write(const uint8_t* buf, size_t size) = 0;
  virtual int available() = 0;
  virtual int read() = 0;
  virtual int read(uint8_t* buf, size_t size) = 0;
  virtual int peek() = 0;
  virtual void flush() = 0;
  virtual void stop() = 0;
  virtual uint8_t connected() = 0;
  virtual operator bool() = 0;
  void setTimeout(unsigned long) {}
};
