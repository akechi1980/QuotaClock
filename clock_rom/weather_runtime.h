// Download/validate off the display task. Only the main loop may draw TFT.
#pragma once
#include "bounded_client.h"
void weaterData(String*, String*, String*, String*);
namespace monoWeather {
struct Job { char city[10]; };
struct Snapshot { char city[10]; bool valid = false; String today, current, forecast, alarm; };
QueueHandle_t jobs = nullptr, results = nullptr;
TaskHandle_t task = nullptr;
bool workerReady = false, inFlight = false, attempted = false, failed = false, repaint = false;
unsigned selfTestsPassed = 0;
uint32_t lastAttempt = 0, lastSuccess = 0;
const char* state = "not_requested";
String activeCity;
Snapshot* cached = nullptr;

// Brace-aware extraction tolerates whitespace/formatting and braces inside
// quoted strings. Missing/truncated variables are rejected, never substring(0).
bool assignedObject(const String& body, const char* name, String& out) {
  int pos = body.indexOf(String("var ") + name);
  if (pos < 0) return false;
  pos += 4 + strlen(name);
  while (pos < (int)body.length() && isspace((unsigned char)body[pos])) ++pos;
  if (body[pos++] != '=') return false;
  while (pos < (int)body.length() && isspace((unsigned char)body[pos])) ++pos;
  if (body[pos] != '{') return false;
  int start = pos, depth = 0; bool quoted = false, escaped = false;
  for (; pos < (int)body.length(); ++pos) {
    char c = body[pos];
    if (quoted) { if (escaped) escaped = false; else if (c == '\\') escaped = true; else if (c == '"') quoted = false; }
    else if (c == '"') quoted = true;
    else if (c == '{') ++depth;
    else if (c == '}' && --depth == 0) { out = body.substring(start, pos + 1); return true; }
  }
  return false;
}
bool compactObject(JsonObjectConst object, const char* const* keys, size_t count, String& out) {
  StaticJsonDocument<768> small;
  for (size_t i = 0; i < count; ++i) {
    JsonVariantConst value = object[keys[i]];
    if (value.isNull()) return false;
    if (!value.is<const char*>() && !value.is<double>()) return false;
    if (value.is<const char*>() && strlen(value.as<const char*>()) > 96) return false;
    small[keys[i]] = value;
  }
  if (small.overflowed()) return false;
  return serializeJson(small, out) > 0;
}
bool parse(const String& body, Snapshot& result) {
  String part;
  DynamicJsonDocument doc(12288);
  if (!assignedObject(body, "dataSK", part) || deserializeJson(doc, part, DeserializationOption::NestingLimit(6))) return false;
  JsonObjectConst sk = doc.as<JsonObjectConst>();
  const char* const currentKeys[] = {"temp", "SD", "cityname", "aqi", "weather", "WD", "WS", "weathercode"};
  if (!compactObject(sk, currentKeys, 8, result.current)) return false;
  const char* code = sk["weathercode"] | "";
  if (strlen(code) != 3 || (code[0] != 'd' && code[0] != 'n') || !isdigit(code[1]) || !isdigit(code[2])) return false;
  if (!assignedObject(body, "cityDZ", part) || deserializeJson(doc, part, DeserializationOption::NestingLimit(6))) return false;
  const char* const todayKeys[] = {"weather"};
  if (!compactObject(doc["weatherinfo"].as<JsonObjectConst>(), todayKeys, 1, result.today)) return false;
  if (!assignedObject(body, "fc", part) || deserializeJson(doc, part, DeserializationOption::NestingLimit(6))) return false;
  const char* const forecastKeys[] = {"fd", "fc"};
  if (!compactObject(doc["f"][0].as<JsonObjectConst>(), forecastKeys, 2, result.forecast)) return false;
  if (!assignedObject(body, "alarmDZ", part) || deserializeJson(doc, part, DeserializationOption::NestingLimit(6)) || !doc["w"].is<JsonArray>()) return false;
  result.alarm = "{}";
  if (doc["w"].size()) {
    const char* const alarmKeys[] = {"w4", "w5", "w6"};
    if (!compactObject(doc["w"][0].as<JsonObjectConst>(), alarmKeys, 3, result.alarm)) return false;
  }
  return true;
}
bool selfTest() {
  Snapshot result;
  const String good = R"(var cityDZ = {"weatherinfo":{"weather":"clear"}};var alarmDZ = {"w":[]};var dataSK = {"temp":"20","SD":"50%","cityname":"test","aqi":"30","weather":"clear","WD":"N","WS":"1","weathercode":"d00"};var fc = {"f":[{"fd":"10","fc":"22"}]};)";
  if (!parse(good, result) || result.alarm != "{}") return false;
  ++selfTestsPassed;
  if (parse("", result)) return false; ++selfTestsPassed;
  if (parse(good.substring(0, good.length() - 6), result)) return false; ++selfTestsPassed;
  String bad = good; bad.replace("\"d00\"", "\"invalid\"");
  if (parse(bad, result)) return false; ++selfTestsPassed;
  bad = good; bad.replace("\"temp\":\"20\",", "");
  if (parse(bad, result)) return false; ++selfTestsPassed;
  bad = good; bad.replace("\"w\":[]", "\"w\":{}");
  if (parse(bad, result)) return false; ++selfTestsPassed;
  bad = good; bad.replace("\"w\":[]", "\"w\":[{\"w4\":7,\"w5\":\"heat\",\"w6\":2}]");
  if (!parse(bad, result) || result.alarm.indexOf("heat") < 0) return false; ++selfTestsPassed;
  String object;
  if (!assignedObject(R"(var dataSK = {"x":"}\\\"{"};)", "dataSK", object)) return false; ++selfTestsPassed;
  return true;
}
class Body : public Stream {
public:
  String text; bool ok = true;
  size_t write(uint8_t b) override { return write(&b, 1); }
  size_t write(const uint8_t* p, size_t n) override {
    if (text.length() + n > 24576 || !text.concat((const char*)p, n)) { ok = false; return 0; }
    return n;
  }
  int available() override { return 0; } int read() override { return -1; }
  int peek() override { return -1; } void flush() override {}
};
void worker(void*) {
  Job job;
  for (;;) {
    if (xQueueReceive(jobs, &job, portMAX_DELAY) != pdTRUE) continue;
    Snapshot* result = new (std::nothrow) Snapshot;
    if (result) {
      memcpy(result->city, job.city, sizeof(job.city));
      BoundedClient client(10000); HTTPClient http;
      http.setConnectTimeout(3000); http.setTimeout(3000); http.setReuse(false);
      String url = String("http://d1.weather.com.cn/weather_index/") + job.city + ".html?_=" + String((uint32_t)time(nullptr));
      if (http.begin(client, url)) {
        http.setUserAgent("Mozilla/5.0"); http.addHeader("Referer", "http://www.weather.com.cn/");
        int code = http.GET();
        if (code == 200 && http.getSize() <= 24576) {
          Body body; int read = http.writeToStream(&body);
          result->valid = read >= 0 && body.ok && parse(body.text, *result);
        }
        http.end();
      }
    }
    // A null result explicitly reports allocation failure and releases inFlight.
    if (xQueueSend(results, &result, 0) != pdTRUE) delete result;
  }
}
void begin() {
  if (!selfTest()) { state = "self_test_failed"; return; }
  jobs = xQueueCreate(1, sizeof(Job)); results = xQueueCreate(1, sizeof(Snapshot*));
  workerReady = jobs && results && xTaskCreatePinnedToCore(worker, "weather-http", 8192, nullptr, 1, &task, 0) == pdPASS;
  if (!workerReady) state = "worker_unavailable";
}
void tick(bool enabled, const String& city, uint32_t intervalMinutes) {
  if (activeCity != city) { activeCity = city; attempted = false; delete cached; cached = nullptr; }
  Snapshot* result = nullptr;
  if (results && xQueueReceive(results, &result, 0) == pdTRUE) {
    inFlight = false;
    if (result && city == result->city && result->valid) {
      delete cached; cached = result; result = nullptr;
      failed = false; repaint = true; lastSuccess = millis(); state = "ok";
    } else if (!result || city == result->city) { failed = true; state = "failed_keep_previous"; }
    delete result;
  }
  uint32_t wait = failed ? 60000UL : intervalMinutes * 60000UL;
  if (enabled && workerReady && !inFlight && (!attempted || millis() - lastAttempt >= wait)) {
    Job job{}; city.toCharArray(job.city, sizeof(job.city));
    if (xQueueSend(jobs, &job, 0) == pdTRUE) { inFlight = true; attempted = true; lastAttempt = millis(); state = "fetching"; }
  }
}
void paint() {
  if (!repaint || !cached) return;
  repaint = false;
  weaterData(&cached->today, &cached->current, &cached->forecast, &cached->alarm);
}
} // namespace monoWeather
