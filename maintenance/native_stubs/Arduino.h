// Host-only test adapter. Never included by the ESP32 build.
#pragma once
#include <algorithm>
#include <cstdint>
#include <cstring>
#include <string>
#include <cctype>
using std::min;
class String {
  std::string text;
public:
  String() = default;
  String(const char* value) : text(value ? value : "") {}
  String(const std::string& value) : text(value) {}
  const char* c_str() const { return text.c_str(); }
  size_t length() const { return text.size(); }
  char operator[](size_t i) const { return i < text.size() ? text[i] : 0; }
  bool concat(const char* p, size_t n) { text.append(p, n); return true; }
  bool concat(const char* p) { text.append(p); return true; }
  bool concat(char c) { text.push_back(c); return true; }
  int indexOf(const String& value) const { auto p = text.find(value.text); return p == std::string::npos ? -1 : (int)p; }
  String substring(size_t a, size_t b = std::string::npos) const { return a >= text.size() ? String() : String(text.substr(a, b == std::string::npos ? b : b-a)); }
  void replace(const String& from, const String& to) { size_t p = 0; while ((p = text.find(from.text, p)) != std::string::npos) { text.replace(p, from.length(), to.text); p += to.length(); } }
  friend String operator+(const String& a, const String& b) { return a.text + b.text; }
  String& operator+=(const String& b) { text += b.text; return *this; }
  bool operator==(const String& b) const { return text == b.text; }
  bool operator!=(const String& b) const { return !(*this == b); }
};
class StringSumHelper : public String {};
extern uint32_t fakeMillis;
inline uint32_t millis() { return fakeMillis; }
inline void vTaskDelay(uint32_t ms) { fakeMillis += ms ? ms : 1; }
#define pdMS_TO_TICKS(ms) (ms)
