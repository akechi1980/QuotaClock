// Monotonic, wrap-safe polling decisions; independent of wall-clock/NTP time.
#pragma once
#include <stdint.h>

namespace monoPoll {
constexpr uint32_t timeoutMs = 3000;
constexpr uint32_t retryMs = 10000;
constexpr bool due(uint32_t now, uint32_t lastAttempt, uint32_t lastFailure,
                   uint32_t interval, bool retry, bool inFlight) {
  return !inFlight && (uint32_t)(now - (retry ? lastFailure : lastAttempt)) >= (retry ? retryMs : interval);
}
constexpr bool healthyAfterResult(bool success, bool wasRetry, bool previousHealthy) {
  return success ? true : (wasRetry ? false : previousHealthy);
}
constexpr bool retryAfterResult(bool success, bool wasRetry) {
  return !success && !wasRetry;
}
static_assert(!due(9999, 0, 0, 300000, true, false), "Do not retry before ten seconds");
static_assert(due(10000, 0, 0, 300000, true, false), "Retry at ten seconds after failure");
static_assert(!due(10000, 0, 0, 300000, false, false), "Success restores the normal interval");
static_assert(due(300000, 0, 0, 300000, false, false), "Normal interval remains five minutes");
static_assert(!due(300000, 0, 0, 300000, true, true), "Never overlap an in-flight request");
static_assert(due(0x2700, 0, 0xfffffff0, 300000, true, false), "Retry across millis rollover");
static_assert(!due(0x26ff, 0, 0xfffffff0, 300000, true, false), "Rollover must not retry early");
static_assert(healthyAfterResult(false, false, true), "First failure preserves healthy state");
static_assert(!healthyAfterResult(false, false, false), "First failure must not invent healthy data");
static_assert(!healthyAfterResult(false, true, true), "Failed retry marks the source unhealthy");
static_assert(healthyAfterResult(true, true, false), "Successful retry recovers health");
static_assert(retryAfterResult(false, false), "Initial failure schedules one retry");
static_assert(!retryAfterResult(false, true), "No repeated ten-second retry loop");
static_assert(!retryAfterResult(true, false), "Successful request keeps normal polling");
}
