// Per-transaction transport deadline. DNS remains the core's bounded resolver
// (up to 16s serialization + 15s lookup), and runs only in background workers.
#pragma once
#include <WiFi.h>
#include <sys/socket.h>
#include <errno.h>
class BoundedClient : public WiFiClient {
  uint32_t started, budget;
  size_t received = 0;
  static constexpr size_t byteLimit = 32768;
  bool expired() const { return (uint32_t)(millis() - started) >= budget; }
  int remaining() const { uint32_t age = millis() - started; return age >= budget ? 0 : budget - age; }
public:
  explicit BoundedClient(uint32_t milliseconds) : started(millis()), budget(milliseconds) {}
  int connect(IPAddress ip, uint16_t port) override { return connect(ip, port, 3000); }
  int connect(IPAddress ip, uint16_t port, int32_t timeout) override {
    int left = remaining();
    return left > 0 ? WiFiClient::connect(ip, port, min((int)timeout, left)) : 0;
  }
  int connect(const char* host, uint16_t port) override { return connect(host, port, 3000); }
  int connect(const char* host, uint16_t port, int32_t timeout) override {
    IPAddress ip;
    if (expired()) return 0;
    if (!ip.fromString(host) && WiFi.hostByName(host, ip) != 1) return 0;
    // Restart the transport budget AFTER the bounded DNS lookup, so a normal
    // cache miss does not consume all TCP time. No DNS is done on the UI task.
    started = millis();
    return connect(ip, port, timeout);
  }
  uint8_t connected() override { if (expired()) { stop(); return 0; } return WiFiClient::connected(); }
  int available() override { if (expired() || received >= byteLimit) { stop(); return 0; } return min(WiFiClient::available(), (int)(byteLimit - received)); }
  int read() override { uint8_t byte; return read(&byte, 1) == 1 ? byte : -1; }
  int read(uint8_t* bytes, size_t count) override {
    if (expired() || received >= byteLimit) { stop(); return -1; }
    int n = WiFiClient::read(bytes, min(count, byteLimit - received));
    if (n > 0) received += n;
    return n;
  }
  int peek() override { if (expired()) { stop(); return -1; } return WiFiClient::peek(); }
  size_t write(uint8_t byte) override { return write(&byte, 1); }
  size_t write(const uint8_t* bytes, size_t count) override {
    size_t sent = 0;
    while (sent < count && !expired()) {
      int n = ::send(fd(), bytes + sent, min((size_t)1024, count - sent), MSG_DONTWAIT);
      if (n > 0) sent += n;
      else if (n < 0 && errno != EAGAIN && errno != EWOULDBLOCK && errno != EINTR) break;
      else vTaskDelay(pdMS_TO_TICKS(1));
    }
    if (sent != count) stop();
    return sent;
  }
};
