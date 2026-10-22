#pragma once
#include "Arduino.h"
class IPAddress {
public:
  bool fromString(const char* host) { return std::isdigit((unsigned char)host[0]); }
};
struct FakeWiFi {
  uint32_t dnsDelay = 0; bool dnsOK = true;
  int hostByName(const char*, IPAddress&) { fakeMillis += dnsDelay; return dnsOK ? 1 : 0; }
};
extern FakeWiFi WiFi;
class WiFiClient {
public:
  bool open = false; int connectTimeout = 0;
  virtual ~WiFiClient() = default;
  virtual int connect(IPAddress, uint16_t) { open = true; return 1; }
  virtual int connect(IPAddress, uint16_t, int32_t timeout) { connectTimeout = timeout; open = true; return 1; }
  virtual int connect(const char*, uint16_t) { return 1; }
  virtual int connect(const char*, uint16_t, int32_t) { return 1; }
  virtual uint8_t connected() { return open; }
  virtual int available() { return open ? 65536 : 0; }
  virtual int read() { return open ? 'x' : -1; }
  virtual int read(uint8_t* p, size_t n) { if (!open) return -1; memset(p, 'x', n); return (int)n; }
  virtual int peek() { return open ? 'x' : -1; }
  virtual size_t write(uint8_t) { return 1; }
  virtual size_t write(const uint8_t*, size_t n) { return n; }
  virtual void stop() { open = false; }
  int fd() const { return open ? 1 : -1; }
};
