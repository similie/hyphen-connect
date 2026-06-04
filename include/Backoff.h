// Backoff.h — dependency-free exponential-backoff schedule.
//
// Pure function of plain integers (no Arduino, no millis()), so it unit-tests on
// the host. Used to space out reconnect attempts: a device that loses the broker
// or cellular coverage must keep retrying forever (giving up = a dead field
// device), but it must not hammer the broker/modem every ~500 ms and tear down
// the whole stack each cycle — that is a self-inflicted reconnect storm and a
// power sink. Unlimited attempts, but a bounded, growing delay between them.
#pragma once

#include <stdint.h>

namespace hyphen {
namespace backoff {

// Exponential backoff with a cap: attempt 0 -> baseMs, doubling each attempt,
// never exceeding maxMs. The loop (rather than `baseMs << attempt`) avoids
// undefined/overflow behavior for large attempt counts.
inline uint32_t delayMs(uint32_t attempt, uint32_t baseMs, uint32_t maxMs) {
  uint32_t d = baseMs;
  for (uint32_t i = 0; i < attempt && d < maxMs; i++) {
    d <<= 1;
  }
  return d > maxMs ? maxMs : d;
}

}  // namespace backoff
}  // namespace hyphen
