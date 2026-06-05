// ArduinoLog.h — native no-op shim for the thijse/ArduinoLog API.
// Swallows all log calls so the code under test runs silently in CI.
#pragma once

#include <Arduino.h>

#ifndef CR
#define CR "\r\n"
#endif

// Log level constants (match ArduinoLog) in case a TU references them.
#define LOG_LEVEL_SILENT 0
#define LOG_LEVEL_FATAL 1
#define LOG_LEVEL_ERROR 2
#define LOG_LEVEL_WARNING 3
#define LOG_LEVEL_NOTICE 4
#define LOG_LEVEL_INFO 4
#define LOG_LEVEL_TRACE 5
#define LOG_LEVEL_VERBOSE 6

struct LoggingShim {
  void begin(int, void* = nullptr, bool = true) {}
  void setLevel(int) {}
  template <typename... A> void trace(A&&...) {}
  template <typename... A> void traceln(A&&...) {}
  template <typename... A> void notice(A&&...) {}
  template <typename... A> void noticeln(A&&...) {}
  template <typename... A> void info(A&&...) {}
  template <typename... A> void infoln(A&&...) {}
  template <typename... A> void warning(A&&...) {}
  template <typename... A> void warningln(A&&...) {}
  template <typename... A> void error(A&&...) {}
  template <typename... A> void errorln(A&&...) {}
  template <typename... A> void fatal(A&&...) {}
  template <typename... A> void fatalln(A&&...) {}
  template <typename... A> void verbose(A&&...) {}
  template <typename... A> void verboseln(A&&...) {}
};
inline LoggingShim Log;
