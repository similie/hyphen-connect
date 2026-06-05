// Ticker.h — native no-op shim for the ESP32 Ticker.
//
// In tests the timer-driven registration path does nothing on its own; tests
// drive the registration/keep-alive state directly (via the HYPHEN_NATIVE_TEST
// seams) so timing is deterministic rather than wall-clock dependent.
#pragma once

class Ticker {
 public:
  Ticker() = default;

  template <typename TArg>
  void attach(float, void (*)(TArg), TArg) {}
  void attach(float, void (*)()) {}

  template <typename TArg>
  void once(float, void (*)(TArg), TArg) {}
  void once(float, void (*)()) {}

  void detach() {}
  bool active() const { return false; }
};
