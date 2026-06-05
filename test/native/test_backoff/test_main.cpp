// Native tests for the reconnect backoff schedule (include/Backoff.h).
//
// Locks in the fix for the reconnect storm: HyphenRunner::initManager() used a
// fixed coreDelay(500) between reconnect attempts, so a broker/coverage outage
// meant retrying ~2x/second forever (tearing down and rebuilding the whole stack
// each cycle). It now spaces attempts with this exponential, capped schedule.
#include <unity.h>

#include "Backoff.h"

using hyphen::backoff::delayMs;

static const uint32_t BASE = 500;
static const uint32_t MAX = 30000;

void setUp() {}
void tearDown() {}

void test_first_attempt_is_base() {
  TEST_ASSERT_EQUAL_UINT32(BASE, delayMs(0, BASE, MAX));
}

void test_doubles_each_attempt() {
  TEST_ASSERT_EQUAL_UINT32(1000, delayMs(1, BASE, MAX));
  TEST_ASSERT_EQUAL_UINT32(2000, delayMs(2, BASE, MAX));
  TEST_ASSERT_EQUAL_UINT32(4000, delayMs(3, BASE, MAX));
  TEST_ASSERT_EQUAL_UINT32(8000, delayMs(4, BASE, MAX));
  TEST_ASSERT_EQUAL_UINT32(16000, delayMs(5, BASE, MAX));
}

void test_caps_at_max() {
  TEST_ASSERT_EQUAL_UINT32(MAX, delayMs(6, BASE, MAX));   // 32000 -> capped
  TEST_ASSERT_EQUAL_UINT32(MAX, delayMs(10, BASE, MAX));  // stays capped
}

// Must not overflow / wrap for absurd attempt counts.
void test_huge_attempt_count_stays_capped() {
  TEST_ASSERT_EQUAL_UINT32(MAX, delayMs(100000, BASE, MAX));
}

// If base already exceeds max, return the cap (never larger than max).
void test_base_above_max_is_clamped() {
  TEST_ASSERT_EQUAL_UINT32(MAX, delayMs(0, 40000, MAX));
}

// The schedule must be monotonically non-decreasing.
void test_monotonic_non_decreasing() {
  uint32_t prev = 0;
  for (uint32_t a = 0; a < 20; a++) {
    uint32_t d = delayMs(a, BASE, MAX);
    TEST_ASSERT_TRUE(d >= prev);
    prev = d;
  }
}

int main(int, char**) {
  UNITY_BEGIN();
  RUN_TEST(test_first_attempt_is_base);
  RUN_TEST(test_doubles_each_attempt);
  RUN_TEST(test_caps_at_max);
  RUN_TEST(test_huge_attempt_count_stays_capped);
  RUN_TEST(test_base_above_max_is_clamped);
  RUN_TEST(test_monotonic_non_decreasing);
  return UNITY_END();
}
