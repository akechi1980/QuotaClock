// Connection state decisions, independent of HTTP quota data and wall-clock time.
#pragma once
#include <stdint.h>
namespace monoWifi {
enum SavedState { Unknown = -1, Missing = 0, Present = 1 };
constexpr uint32_t connectGraceMs = 15000;
constexpr uint32_t retryMs = 30000;
constexpr bool openPortal(bool connected, bool portalActive, int saved, uint32_t disconnectedMs) {
  return !connected && !portalActive && (saved == Missing || disconnectedMs >= connectGraceMs);
}
constexpr bool closePortal(bool portalActive, bool manual, bool connected) {
  return portalActive && !manual && connected;
}
constexpr bool retrySaved(bool portalActive, bool manual, bool connected, int saved,
                          unsigned apClients, uint32_t elapsedMs) {
  return portalActive && !manual && !connected && saved == Present && apClients == 0 && elapsedMs >= retryMs;
}
static_assert(!openPortal(false, false, Present, 0), "Saved Wi-Fi gets time to connect");
static_assert(!openPortal(false, false, Present, 14999), "Do not open the portal early");
static_assert(openPortal(false, false, Present, 15000), "Fallback after connection grace");
static_assert(!openPortal(false, false, Unknown, 0), "Unknown config is not empty config");
static_assert(openPortal(false, false, Missing, 0), "Missing credentials allow immediate setup");
static_assert(!openPortal(true, false, Present, 60000), "Connected device never falls back for quota failure");
static_assert(closePortal(true, false, true), "Recovered Wi-Fi leaves automatic setup");
static_assert(!closePortal(true, true, true), "Manual reconfiguration stays open until save/exit");
static_assert(retrySaved(true, false, false, Present, 0, 30000), "Retry saved credentials while in fallback");
static_assert(!retrySaved(true, false, false, Present, 1, 30000), "Do not interrupt a phone configuring Wi-Fi");
static_assert(!retrySaved(true, false, false, Missing, 0, 30000), "Never connect with empty credentials");
static_assert(!retrySaved(true, true, false, Present, 0, 30000), "Manual portal owns its connection attempts");
}
