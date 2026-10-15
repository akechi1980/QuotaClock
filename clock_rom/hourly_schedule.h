// Five-second hourly grace, without replaying old hours after clock jumps.
#pragma once
#include <stdint.h>
namespace monoHourly {
constexpr int64_t windowSeconds = 5;
constexpr bool elapsedMatches(int64_t seconds, uint32_t elapsedMs) {
  return seconds * 1000 <= (int64_t)elapsedMs + 2000 && (int64_t)elapsedMs <= seconds * 1000 + 2000;
}
constexpr bool due(int64_t epoch, int64_t previous, int64_t highestHour, uint32_t elapsedMs, bool allowed) {
  return allowed && previous > 0 && epoch >= previous && epoch - previous <= windowSeconds + 1 &&
         epoch / 3600 > highestHour && epoch % 3600 <= windowSeconds && elapsedMatches(epoch - previous, elapsedMs);
}
constexpr int64_t testHour = 1800000000;
static_assert(due(testHour, testHour-1, testHour/3600-1, 1000, true), "Trigger at the hour");
static_assert(due(testHour+4, testHour-1, testHour/3600-1, 5000, true), "Tolerate a five-second loop delay");
static_assert(due(testHour+5, testHour-1, testHour/3600-1, 6000, true), "Include second five");
static_assert(!due(testHour+6, testHour-1, testHour/3600-1, 7000, true), "Do not replay outside grace");
static_assert(!due(testHour+1, testHour, testHour/3600, 1000, true), "Never duplicate an hour");
static_assert(!due(testHour, 0, 0, 1000, true), "No startup chime");
static_assert(!due(testHour, testHour-3600, testHour/3600-1, 1000, true), "Do not replay a large clock jump");
static_assert(!due(testHour+4, testHour-1, testHour/3600-1, 10, true), "Reject fast wall-clock correction");
static_assert(!due(testHour-1, testHour, testHour/3600, 1000, true), "Reject backward clock movement");
static_assert(!due(testHour, testHour-1, testHour/3600-1, 1000, false), "Respect disabled/boot/portal state");
}
