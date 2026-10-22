// V1.4 development extension. Included after the original global declarations.
// Original author attribution remains in clock_rom.ino; verified originals untouched.
#pragma once
#include <Preferences.h>
#include <time.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>
#include "management_page.h"
#include "ui_glyphs.h"
#include "mono_audio.h"
#include "poll_schedule.h"
#include "hourly_schedule.h"
#include "wifi_recovery.h"
#include <esp_wifi.h>
#include "bounded_client.h"
#include "weather_runtime.h"

namespace mono {
constexpr const char* version = "SDD MONO 0.2.6-dev";
const char* const taskNames[] = {"OpenAI 5 Hours", "OpenAI Week", "Cursor Month"};
struct Options { String url; uint32_t intervalMinutes = 5; bool progress = true; bool hourly = false; uint8_t volume = 30; uint8_t beeps = 3; } options;
struct Job { char url[193]; uint32_t revision; };
struct Result { int code; bool valid; bool simulation; uint8_t values[3]; uint32_t revision; };
QueueHandle_t jobs = nullptr, results = nullptr;
TFT_eSprite frame(&tft);
TFT_eSprite calendar(&tft); // Reuse V1.4's existing Chinese font; no new font blob.
String accessKey, csrf, apName;
bool serverStarted = false, portalRequested = false, received = false, simulated = false;
bool routesRegistered = false, manualPortal = false;
bool pollWorkerReady = false;
TaskHandle_t pollTask = nullptr;
uint32_t maxLoopMs = 0;
int storedWifiState = monoWifi::Unknown;
uint32_t lastWifiAttempt = 0;
bool pollHealthy = false;
bool retryPending = false, requestInFlight = false, requestWasRetry = false;
uint32_t lastFailure = 0;
uint8_t values[3] = {0, 0, 0};
uint32_t lastValue = 0, lastRequest = 0, revision = 1, bootUntil = 0;
String sourceState = "DEMO";
bool legacyNeedsRefresh = false;
uint32_t reminderStarted = 0;
bool reminderActive = false;
time_t lastClock = 0, highestHour = 0;
uint32_t lastClockTick = 0;
uint8_t reminderBeeps = 3;
time_t lastHourlyEpoch = 0;
const char* lastHourlyAudio = "not_triggered";
uint32_t lastAudition = 0;
const uint16_t background = 0x29A7, muted = 0x8CB3, ink = 0xBE18, lit = 0xA597, unlit = 0x426A;
constexpr int taskTop = 100, taskStep = 42, barOffset = 18, footerTop = 218;
static_assert(taskTop + 2 * taskStep + barOffset + 9 <= footerTop - 4, "Progress and footer must not overlap");
static_assert(footerTop + 8 <= 240 - 14, "Preserve bottom breathing room");

String randomKey() {
  char out[17];
  snprintf(out, sizeof(out), "%08lx%08lx", (unsigned long)esp_random(), (unsigned long)esp_random());
  return String(out);
}
String numericApKey() {
  // Uniform eight-digit decimal key (including leading zeroes), WPA2 minimum.
  uint32_t value;
  do { value = esp_random(); } while (value >= 4200000000UL);
  char out[9]; snprintf(out, sizeof(out), "%08lu", (unsigned long)(value % 100000000UL));
  return String(out);
}
bool validApKey(const String& key) {
  if (key.length() != 8) return false;
  for (unsigned i = 0; i < key.length(); ++i) if (key[i] < '0' || key[i] > '9') return false;
  return true;
}
bool validUrl(const String& url) {
  if (!url.length()) return true;
  if (url.length() > 192 || !url.startsWith("http://")) return false;
  for (unsigned i = 0; i < url.length(); ++i)
    if ((uint8_t)url[i] <= 32 || (uint8_t)url[i] >= 127 || url[i] == '@' || url[i] == '#') return false;
  unsigned slash = url.indexOf('/', 7);
  String host = slash == (unsigned)-1 ? url.substring(7) : url.substring(7, slash);
  return host.length() > 0;
}
bool saveOptions(const Options& value) {
  Preferences p;
  if (!p.begin("mono-v14", false)) return false;
  StaticJsonDocument<512> doc;
  doc["url"] = value.url; doc["interval_minutes"] = value.intervalMinutes; doc["progress"] = value.progress;
  doc["hourly_enabled"] = value.hourly; doc["hourly_volume"] = value.volume;
  doc["hourly_beeps"] = value.beeps;
  String data; serializeJson(doc, data);
  bool ok = p.putString("settings", data) == data.length(); p.end(); return ok;
}
void begin() {
  Preferences p;
  if (p.begin("mono-v14", false)) {
    accessKey = p.getString("access", "");
    if (!validApKey(accessKey)) { accessKey = numericApKey(); p.putString("access", accessKey); }
    StaticJsonDocument<512> doc;
    if (!deserializeJson(doc, p.getString("settings", ""))) {
      String url = doc["url"] | "";
      uint32_t minutes = doc["interval_minutes"] | 5U;
      if (validUrl(url) && minutes >= 1 && minutes <= 60) {
        options.url = url; options.intervalMinutes = minutes; options.progress = doc["progress"] | true;
        options.hourly = doc["hourly_enabled"] | false;
        unsigned volume = doc["hourly_volume"] | 30U;
        options.volume = volume <= 100 ? volume : 30;
        unsigned beeps = doc["hourly_beeps"] | 3U;
        options.beeps = beeps >= 1 && beeps <= 5 ? beeps : 3;
      }
    }
    p.end();
  }
  // Keep the setup AP password-protected even if NVS initialization failed.
  if (!validApKey(accessKey)) accessKey = numericApKey();
  csrf = randomKey();
  apName = "Clock-" + WiFi.macAddress().substring(12);
  apName.replace(":", "");
  frame.setColorDepth(16); frame.createSprite(240, 240);
  if (!frame.created()) {
    tft.fillScreen(TFT_BLACK); tft.setTextColor(TFT_WHITE, TFT_BLACK);
    tft.drawString("Display memory unavailable", 8, 80, 2);
    tft.drawString("Web management still available", 8, 110, 2);
  }
  calendar.setColorDepth(16);
  if (calendar.createSprite(240, 38)) calendar.loadFont(ZdyLwFont_20);
  jobs = xQueueCreate(1, sizeof(Job)); results = xQueueCreate(1, sizeof(Result));
  monoAudio::begin();
}
bool progressEnabled() { return options.progress; }
uint32_t pollIntervalMs() { return options.intervalMinutes * 60000UL; }
bool dataStale() { return received && millis() - lastValue >= pollIntervalMs() * 2; }
bool online() {
  return WiFi.status() == WL_CONNECTED &&
         (!options.url.length() || (received && pollHealthy && !dataStale()));
}
bool showingBoot() { return reminderActive || wm.getConfigPortalActive() || (bootUntil && (int32_t)(millis() - bootUntil) < 0); }

void uiText(const char* text, int x, int y, int size = 20) {
  const uint8_t* p = (const uint8_t*)text;
  while (*p) {
    uint32_t code = *p++;
    if (code >= 0xe0 && *p && p[1]) { code = (code & 15) << 12 | (p[0] & 63) << 6 | (p[1] & 63); p += 2; }
    const MonoGlyph* glyph = nullptr;
    for (auto& g : monoGlyphs) if (g.code == code) { glyph = &g; break; }
    if (glyph) for (int yy = 0; yy < size; ++yy) for (int xx = 0; xx < size; ++xx) {
      int sx = xx * 20 / size, sy = yy * 20 / size;
      if (glyph->bits[sy*3 + sx/8] & (0x80 >> (sx%8))) frame.drawPixel(x+xx, y+yy, ink);
    }
    x += size;
  }
}

void boot(const String& title, const String& first, const String& second, unsigned percent) {
  if (!frame.created()) return;
  frame.fillSprite(background); frame.setTextDatum(TL_DATUM);
  frame.setTextColor(ink, background);
  if (wm.getConfigPortalActive()) {
    uiText("连接热点", 18, 16, 25);
    frame.drawString(apName, 18, 55, 4);
    uiText("热点密码", 18, 101, 16);
    frame.drawString(accessKey, 18, 126, 4);
    frame.drawFastHLine(18, 176, 204, unlit);
    uiText("浏览器打开", 18, 184, 14);
    frame.drawString("192.168.4.1", 18, 208, 4);
  } else if (WiFi.status() == WL_CONNECTED) {
    uiText("连接网络", 64, 42, 28);
    frame.drawCentreString("OK", 120, 88, 4);
    uiText("浏览器打开", 70, 144, 20);
    frame.drawCentreString(WiFi.localIP().toString(), 120, 180, 4);
  } else {
    frame.drawCentreString("MONO", 120, 60, 4);
    uiText(title == "CLOCK / PROGRESS" ? "正在启动" : "连接网络", 72, 116, 24);
    frame.fillRect(44, 183, 152, 3, unlit);
    frame.fillRect(44, 183, 152 * min(percent, 100U) / 100, 3, lit);
  }
  frame.pushSprite(0, 0);
}
void calendarText(const char* text, bool rightAligned, int scale = 7) {
  // Render using the existing 20px Chinese font, then shrink to about 14px.
  // Left/right screen anchors remain 16 and 224; no extra font asset required.
  calendar.fillSprite(background); calendar.setTextColor(muted, background);
  calendar.setTextDatum(TL_DATUM); calendar.drawString(text, 0, 0);
  int width = min(240, (int)calendar.textWidth(text));
  int height = min(38, (int)calendar.fontHeight());
  int targetWidth = (width * scale + 9) / 10, targetHeight = (height * scale + 9) / 10;
  int left = rightAligned ? 224 - targetWidth : 16;
  for (int y = 0; y < targetHeight; ++y)
    for (int x = 0; x < targetWidth; ++x)
      frame.drawPixel(left + x, 15 + y, calendar.readPixel(x * 10 / scale, y * 10 / scale));
}
void drawProgress() {
  if (!frame.created()) return;
  frame.fillSprite(background);
  time_t epoch = time(nullptr); struct tm local{};
  bool synced = epoch >= 946684800 && localtime_r(&epoch, &local);
  char text[32];
  if (calendar.created()) {
    if (synced) snprintf(text, sizeof(text), "%04d年%d月%d日", local.tm_year + 1900, local.tm_mon + 1, local.tm_mday);
    else snprintf(text, sizeof(text), "----年--月--日");
    calendarText(text, false, 9);
    const char* days[] = {"星期日", "星期一", "星期二", "星期三", "星期四", "星期五", "星期六"};
    calendarText(synced ? days[local.tm_wday] : "星期--", true, 10);
  } else {
    frame.setTextColor(muted, background); frame.drawString("CALENDAR UNAVAILABLE", 16, 15, 1);
  }
  snprintf(text, sizeof(text), synced ? "%02d:%02d" : "--:--", local.tm_hour, local.tm_min);
  frame.setTextColor(ink, background); frame.drawCentreString(text, 120, 43, synced ? 7 : 4);
  bool demo = !received || !options.url.length();
  bool stale = !demo && dataStale();
  for (unsigned row = 0; row < 3; ++row) {
    unsigned percent = demo ? (millis() / 1200 + row * 33) % 101 : values[row];
    frame.setTextColor(muted, background);
    frame.drawString(taskNames[row], 16, taskTop + row * taskStep, 1);
    snprintf(text, sizeof(text), stale ? "%u%%*" : "%u%%", percent);
    frame.setTextColor(stale ? muted : ink, background); frame.drawRightString(text, 224, taskTop + row * taskStep, 1);
    for (unsigned n = 0; n < 20; ++n) {
      int left = n * 208 / 20, right = (n + 1) * 208 / 20;
      frame.fillRect(16 + left, taskTop + barOffset + row * taskStep, right - left - 2, 9,
                     n < (percent >= 100 ? 20 : percent / 5) ? lit : unlit);
    }
  }
  frame.setTextColor(muted, background);
  String footer = String(demo ? "DEMO / " : simulated ? "SIM / " : "LIVE / ");
  footer += WiFi.status() == WL_CONNECTED ? WiFi.localIP().toString() : "OFFLINE";
  frame.drawString(footer, 16, footerTop, 1);
  frame.setTextColor(muted, background);
  frame.drawRightString("ONLINE", 214, footerTop, 1);
  frame.fillCircle(222, footerTop + 4, 3, online() ? TFT_GREEN : TFT_RED);
  frame.pushSprite(0, 0);
}

void drawReminder() {
  if (!frame.created()) return;
  frame.fillSprite(background); frame.setTextDatum(TL_DATUM);
  uiText("整点了", 81, 28, 26);
  time_t epoch = time(nullptr); struct tm local{}; localtime_r(&epoch, &local);
  char text[8]; snprintf(text, sizeof(text), "%02d:%02d", local.tm_hour, local.tm_min);
  frame.setTextColor(ink, background); frame.drawCentreString(text, 120, 82, 7);
  uint32_t elapsed = millis() - reminderStarted;
  for (unsigned n = 0; n < reminderBeeps; ++n)
    frame.fillCircle(120-(reminderBeeps-1)*12+n*24, 175, 4, elapsed >= n*500 ? ink : unlit);
  uiText("稍后返回", 92, 207, 14);
  frame.pushSprite(0, 0);
}
void checkHourly() {
  time_t epoch = time(nullptr); struct tm local{};
  if (epoch < 946684800 || !localtime_r(&epoch, &local)) return;
  time_t hour = epoch / 3600;
  uint32_t tickNow = millis();
  bool trigger = monoHourly::due(epoch, lastClock, highestHour, tickNow-lastClockTick,
                                options.hourly && WiFi.status() == WL_CONNECTED && !showingBoot());
  if (hour > highestHour) highestHour = hour;
  lastClock = epoch; lastClockTick = tickNow;
  if (trigger) {
    reminderActive = true; reminderStarted = millis(); reminderBeeps = options.beeps;
    lastHourlyEpoch = epoch;
    lastHourlyAudio = !options.volume ? "muted" : monoAudio::request(options.volume, options.beeps) ? "queued" : "unavailable_or_busy";
  }
}

class LimitedBody : public Stream {
  String body; uint32_t started = millis(); bool failed = false;
public:
  size_t write(uint8_t b) override { return write(&b, 1); }
  size_t write(const uint8_t* bytes, size_t count) override {
    if (body.length() + count > 2048 || millis() - started > 5000) { failed = true; return 0; }
    if (!body.concat((const char*)bytes, count)) { failed = true; return 0; }
    return count;
  }
  int available() override { return 0; } int read() override { return -1; }
  int peek() override { return -1; } void flush() override {}
  const String& text() const { return body; } bool ok() const { return !failed; }
};
void pollWorker(void*) {
  Job job;
  for (;;) {
    if (xQueueReceive(jobs, &job, portMAX_DELAY) != pdTRUE) continue;
    Result result{}; result.revision = job.revision;
    BoundedClient client(10000); HTTPClient http;
    http.setConnectTimeout(monoPoll::timeoutMs); http.setTimeout(monoPoll::timeoutMs); http.setReuse(false);
    if (http.begin(client, job.url)) {
      result.code = http.GET();
      if (result.code == 200 && http.getSize() <= 2048) {
        LimitedBody body;
        int size = http.writeToStream(&body);
        StaticJsonDocument<768> doc;
        if (size >= 0 && body.ok() && !deserializeJson(doc, body.text(), DeserializationOption::NestingLimit(3))) {
          JsonArrayConst array = doc["progress"].as<JsonArrayConst>();
          result.valid = array.size() == 3;
          for (unsigned n = 0; result.valid && n < 3; ++n) {
            if (!array[n].is<int>() || array[n].as<int>() < 0 || array[n].as<int>() > 100) result.valid = false;
            else result.values[n] = array[n].as<int>();
          }
          result.simulation = doc["simulation"] | false;
        }
      }
      http.end();
    } else result.code = -1;
    xQueueOverwrite(results, &result);
  }
}
void startPolling() {
  pollWorkerReady = jobs && results && xTaskCreatePinnedToCore(pollWorker, "progress-http", 8192, nullptr, 1, &pollTask, 0) == pdPASS;
  if (!pollWorkerReady) sourceState = "worker unavailable";
}
bool authenticate(bool write = false) {
  // User requested no management login in this development version.
  // The AP's WPA2 key is separate; this nonce only blocks cross-origin writes.
  if (write && server.header("X-CP-Token") != csrf) { server.send(403, "application/json", "{\"error\":\"invalid CSRF token\"}"); return false; }
  server.sendHeader("Cache-Control", "no-store"); return true;
}
void jsonError(int code, const char* message) {
  StaticJsonDocument<256> doc; doc["error"] = message;
  String response; serializeJson(doc, response); server.send(code, "application/json", response);
}
void status() {
  if (!authenticate()) return;
  StaticJsonDocument<2048> doc;
  doc["version"] = version; doc["connected"] = WiFi.status() == WL_CONNECTED;
  doc["ip"] = WiFi.localIP().toString(); doc["csrf"] = csrf;
  doc["brightness"] = LCD_BL_PWM; doc["rotation"] = LCD_Rotation;
  doc["url"] = options.url; doc["interval_minutes"] = options.intervalMinutes;
  doc["mode"] = options.progress ? "progress" : "weather";
  doc["city"] = cityCode; doc["weather_interval"] = updateweater_time;
  doc["source_state"] = sourceState;
  doc["online"] = online();
  doc["http_timeout_ms"] = monoPoll::timeoutMs; doc["retry_delay_ms"] = monoPoll::retryMs;
  doc["retry_pending"] = retryPending; doc["retry_in_flight"] = requestInFlight && requestWasRetry;
  doc["hourly_enabled"] = options.hourly; doc["hourly_volume"] = options.volume;
  doc["hourly_beeps"] = options.beeps; doc["hourly_window_seconds"] = (unsigned)monoHourly::windowSeconds;
  doc["clock_synced"] = time(nullptr) >= 946684800;
  doc["last_hourly_epoch"] = (uint32_t)lastHourlyEpoch; doc["last_hourly_audio"] = lastHourlyAudio;
  doc["audio_state"] = monoAudio::state < 0 ? "unavailable" : monoAudio::state > 0 ? "ready" : "not_tested";
  doc["audio_busy"] = monoAudio::busy;
  doc["wifi_saved_config"] = storedWifiState == monoWifi::Present ? "present" : storedWifiState == monoWifi::Missing ? "missing" : "unknown";
  doc["wifi_portal_active"] = wm.getConfigPortalActive(); doc["wifi_portal_manual"] = manualPortal;
  doc["loop_max_ms"] = maxLoopMs;
  doc["uptime_ms"] = millis(); doc["free_heap"] = ESP.getFreeHeap(); doc["min_free_heap"] = ESP.getMinFreeHeap();
  doc["poll_stack_free"] = pollTask ? uxTaskGetStackHighWaterMark(pollTask) : 0;
  doc["weather_stack_free"] = monoWeather::task ? uxTaskGetStackHighWaterMark(monoWeather::task) : 0;
  doc["poll_worker_ready"] = pollWorkerReady;
  doc["poll_in_flight"] = requestInFlight;
  doc["weather_state"] = monoWeather::state;
  doc["weather_in_flight"] = monoWeather::inFlight;
  doc["weather_last_success_ms"] = monoWeather::lastSuccess;
  doc["weather_self_tests"] = monoWeather::selfTestsPassed;
  JsonArray list = doc.createNestedArray("tasks");
  bool demo = !received || !options.url.length();
  for (unsigned n = 0; n < 3; ++n) {
    JsonObject row = list.createNestedObject();
    row["label"] = taskNames[n];
    row["mode"] = demo ? "demo" : simulated ? "simulation" : "live";
    row["percent"] = demo ? (millis() / 1200 + n * 33) % 101 : values[n];
    row["stale"] = !demo && dataStale();
  }
  String response; serializeJson(doc, response); server.send(200, "application/json", response);
}
void configure() {
  if (!authenticate(true)) return;
  String body = server.arg("plain");
  if (body.length() > 1024) { jsonError(413, "request too large"); return; }
  StaticJsonDocument<1536> doc;
  if (deserializeJson(doc, body, DeserializationOption::NestingLimit(2)) || !doc.is<JsonObject>()) { jsonError(400, "invalid JSON"); return; }
  if (!doc["brightness"].is<int>() || !doc["rotation"].is<int>() || !doc["interval_minutes"].is<int>() ||
      !doc["weather_interval"].is<int>() || !doc["url"].is<const char*>() || !doc["city"].is<const char*>() || !doc["mode"].is<const char*>()) { jsonError(400, "invalid setting types"); return; }
  int brightness = doc["brightness"], rotation = doc["rotation"], interval = doc["interval_minutes"], weather = doc["weather_interval"];
  String url = doc["url"].as<const char*>(), city = doc["city"].as<const char*>(), mode = doc["mode"].as<const char*>();
  bool cityValid = city.length() == 9;
  for (unsigned i = 0; i < city.length(); ++i) cityValid &= city[i] >= '0' && city[i] <= '9';
  long cityNumber = city.toInt();
  if (brightness < 1 || brightness > 100 || rotation < 0 || rotation > 3 || interval < 1 || interval > 60 ||
      weather < 1 || weather > 60 || !validUrl(url) || !cityValid || cityNumber < 101000000 || cityNumber > 102000000 ||
      (mode != "progress" && mode != "weather")) { jsonError(400, "setting outside supported range"); return; }
  Options candidate; candidate.url = url; candidate.intervalMinutes = interval; candidate.progress = mode == "progress";
  candidate.hourly = options.hourly; candidate.volume = options.volume; candidate.beeps = options.beeps;
  if (doc.containsKey("hourly_enabled")) {
    if (!doc["hourly_enabled"].is<bool>()) { jsonError(400, "invalid hourly toggle"); return; }
    candidate.hourly = doc["hourly_enabled"].as<bool>();
  }
  if (doc.containsKey("hourly_volume")) {
    if (!doc["hourly_volume"].is<int>() || doc["hourly_volume"].as<int>() < 0 || doc["hourly_volume"].as<int>() > 100) { jsonError(400, "invalid volume"); return; }
    candidate.volume = doc["hourly_volume"].as<int>();
  }
  if (doc.containsKey("hourly_beeps")) {
    if (!doc["hourly_beeps"].is<int>() || doc["hourly_beeps"].as<int>() < 1 || doc["hourly_beeps"].as<int>() > 5) { jsonError(400, "invalid beep count"); return; }
    candidate.beeps = doc["hourly_beeps"].as<int>();
  }
  if (!saveOptions(candidate)) { jsonError(500, "cannot save settings"); return; }
  EEPROM.write(BL_addr, brightness); EEPROM.write(Ro_addr, rotation); EEPROM.write(UpWeT_addr, weather);
  // Keep the V1.4 decimal city-code encoding, committing all EEPROM fields together.
  int number = cityNumber;
  for (unsigned n = 0; n < 5; ++n) { EEPROM.write(CC_addr + n, number % 100); number /= 100; }
  if (!EEPROM.commit()) {
    saveOptions(options); // Keep active progress settings on failed legacy commit.
    EEPROM.write(BL_addr, LCD_BL_PWM); EEPROM.write(Ro_addr, LCD_Rotation); EEPROM.write(UpWeT_addr, updateweater_time);
    int previous = cityCode.toInt();
    for (unsigned n = 0; n < 5; ++n) { EEPROM.write(CC_addr + n, previous % 100); previous /= 100; }
    jsonError(500, "cannot commit display settings"); return;
  }
  if (options.url != candidate.url) { received = false; pollHealthy = false; }
  options = candidate; ++revision; lastRequest = millis() - pollIntervalMs();
  retryPending = false; requestInFlight = false;
  if (!options.hourly && reminderActive) { reminderActive = false; legacyNeedsRefresh = !options.progress; }
  LCD_BL_PWM = brightness; LCD_Rotation = rotation; updateweater_time = weather; cityCode = city;
  analogWrite(LCD_BL_PIN, 255 - LCD_BL_PWM); // Preserve the proven V1.4 mapping.
  tft.setRotation(LCD_Rotation); legacyNeedsRefresh = !options.progress;
  server.send(200, "application/json", "{\"ok\":true}");
}
void configureReminder() {
  if (!authenticate(true)) return;
  String body = server.arg("plain"); StaticJsonDocument<128> doc;
  if (body.length() > 128 || deserializeJson(doc, body) || !doc.is<JsonObject>() ||
      doc.size() != 1 || !doc["hourly_enabled"].is<bool>()) { jsonError(400, "invalid reminder toggle"); return; }
  bool enabled = doc["hourly_enabled"].as<bool>();
  if (enabled != options.hourly) {
    Options candidate = options; candidate.hourly = enabled;
    if (!saveOptions(candidate)) { jsonError(500, "cannot save reminder"); return; }
    options = candidate;
    if (!enabled && reminderActive) { reminderActive = false; legacyNeedsRefresh = !options.progress; }
  }
  server.send(200, "application/json", enabled ? "{\"ok\":true,\"hourly_enabled\":true}" : "{\"ok\":true,\"hourly_enabled\":false}");
}
void registerWeb() {
  if (!routesRegistered) {
  const char* headers[] = {"X-CP-Token"}; server.collectHeaders(headers, 1);
  server.on("/", HTTP_GET, [] { if (authenticate()) server.send_P(200, "text/html; charset=utf-8", managementPage); });
  server.on("/api/status", HTTP_GET, status); server.on("/api/config", HTTP_POST, configure);
  server.on("/api/reminder/settings", HTTP_POST, configureReminder);
  server.on("/api/reminder/test", HTTP_POST, [] {
    if (!authenticate(true)) return;
    if (lastAudition && millis() - lastAudition < 3000) { jsonError(429, "wait before retrying"); return; }
    String body = server.arg("plain");
    StaticJsonDocument<128> doc;
    if (body.length() > 128 || deserializeJson(doc, body) || !doc["volume"].is<int>()) { jsonError(400, "invalid volume"); return; }
    int volume = doc["volume"];
    if (doc.containsKey("beeps") && !doc["beeps"].is<int>()) { jsonError(400, "invalid beep count"); return; }
    int beeps = doc["beeps"] | (int)options.beeps;
    if (beeps < 1 || beeps > 5) { jsonError(400, "invalid beep count"); return; }
    if (volume < 0 || volume > 100) { jsonError(400, "invalid volume"); return; }
    if (volume && !monoAudio::request(volume, beeps)) { jsonError(503, "audio unavailable or busy"); return; }
    lastAudition = millis(); server.send(202, "application/json", "{\"ok\":true,\"queued\":true}");
  });
  server.on("/api/portal", HTTP_POST, [] { if (authenticate(true)) { portalRequested = true; server.send(200, "application/json", "{\"ok\":true}"); } });
  server.onNotFound([] { if (authenticate()) jsonError(404, "not found"); });
  if (MDNS.begin("clock-progress")) MDNS.addService("http", "tcp", 80);
  routesRegistered = true;
  }
  server.begin(); serverStarted = true; lastRequest = millis() - pollIntervalMs();
  retryPending = false; requestInFlight = false;
}
int readStoredWifiState() {
  wifi_config_t config{};
  if (esp_wifi_get_config(WIFI_IF_STA, &config) != ESP_OK) return monoWifi::Unknown;
  return config.sta.ssid[0] ? monoWifi::Present : monoWifi::Missing;
}
void startPortal(bool manual = false) {
  if (wm.getConfigPortalActive()) return;
  manualPortal = manual;
  ++revision; requestInFlight = false; retryPending = false;
  if (serverStarted) { server.stop(); serverStarted = false; }
  wm.setConfigPortalBlocking(false); wm.setConnectTimeout(15); wm.setSaveConnectTimeout(15); wm.setDebugOutput(false);
  wm.setClass("invert");
  // This callback executes BEFORE the library registers its default routes.
  // Register blockers first; do not merely hide OTA links in the menu.
  wm.setWebServerCallback([] {
    wm.server->on("/update", HTTP_ANY, [] { wm.server->send(403, "text/plain", "Host-verified firmware writes only"); });
    wm.server->on("/u", HTTP_POST, [] { wm.server->send(403, "text/plain", "Firmware upload disabled"); }, [] {});
    wm.server->on("/wifi", HTTP_GET, [] { wm.server->sendHeader("Location", "/wifinoscan", true); wm.server->send(302, "text/plain", "Use direct Wi-Fi entry"); });
  });
  wm.setCustomHeadElement("<style>body{background:#202427;color:#b9c2c7}div{background:transparent}input,button{border:1px solid #414c53;border-radius:6px;background:#2d3438;color:#b9c2c7}button{background:#a5b1b8;color:#202427}a{color:#b9c2c7}</style>");
  // The original /wifi route synchronously scans before returning the page.
  // Default to direct SSID entry so opening configuration never waits for scan.
  static std::vector<const char*> menu = {"wifinoscan", "exit"};
  wm.setMenu(menu);
  wm.startConfigPortal(apName.c_str(), accessKey.c_str());
  // WiFiManager deliberately disables disconnected STA on entry. Re-enable it
  // only for automatic fallback so saved Wi-Fi can recover without a form save.
  if (!manualPortal && wm.getConfigPortalActive()) {
    WiFi.enableSTA(true); WiFi.setAutoReconnect(true);
    storedWifiState = readStoredWifiState();
    if (storedWifiState == monoWifi::Present) WiFi.begin();
  }
  lastWifiAttempt = millis();
}
void connectSavedWifi() {
  WiFi.setAutoReconnect(true); WiFi.begin(); lastWifiAttempt = millis();
  storedWifiState = readStoredWifiState();
  // SSID() describes a connected AP, not whether NVS contains credentials.
  if (monoWifi::openPortal(WiFi.status() == WL_CONNECTED, false, storedWifiState, 0)) startPortal();
}
void tick() {
  checkHourly();
  if (portalRequested) { portalRequested = false; startPortal(true); }
  if (wm.getConfigPortalActive()) {
    wm.process();
    if (monoWifi::closePortal(wm.getConfigPortalActive(), manualPortal, WiFi.status() == WL_CONNECTED)) wm.stopConfigPortal();
    if (wm.getConfigPortalActive()) {
      if (millis() - lastWifiAttempt >= monoWifi::retryMs) storedWifiState = readStoredWifiState();
      if (monoWifi::retrySaved(true, manualPortal, WiFi.status() == WL_CONNECTED, storedWifiState,
                               WiFi.softAPgetStationNum(), millis() - lastWifiAttempt)) {
        WiFi.enableSTA(true); WiFi.begin(); lastWifiAttempt = millis();
      }
      static uint32_t lastPortalDraw = 0;
      if (!lastPortalDraw || millis() - lastPortalDraw >= 500) {
        boot("SETUP / WI-FI", apName, "http://192.168.4.1", 30);
        lastPortalDraw = millis();
      }
      return;
    }
    manualPortal = false;
  }
  if (WiFi.status() == WL_CONNECTED && !serverStarted) {
    registerWeb(); bootUntil = millis() + 3000;
    configTime(8 * 3600, 0, "ntp.aliyun.com", "pool.ntp.org");
  }
  if (serverStarted) server.handleClient();
  static uint32_t disconnected = millis();
  if (WiFi.status() == WL_CONNECTED) { disconnected = millis(); storedWifiState = monoWifi::Present; }
  else if (monoWifi::openPortal(false, false, storedWifiState, millis() - disconnected)) { startPortal(); disconnected = millis(); return; }
  Result result;
  if (requestInFlight && millis() - lastRequest > 60000) {
    pollHealthy = false; sourceState = "worker overdue"; // Diagnose, never spawn duplicate workers.
  }
  if (results && xQueueReceive(results, &result, 0) == pdTRUE && result.revision == revision) {
    requestInFlight = false;
    pollHealthy = monoPoll::healthyAfterResult(result.valid, requestWasRetry, pollHealthy);
    retryPending = monoPoll::retryAfterResult(result.valid, requestWasRetry);
    if (retryPending) lastFailure = millis();
    if (result.valid) { memcpy(values, result.values, 3); received = true; simulated = result.simulation; lastValue = millis(); sourceState = simulated ? "SIM / test source" : "OK"; }
    else {
      String error = result.code == 200 ? "invalid JSON" : "HTTP " + String(result.code);
      sourceState = (retryPending ? "RETRY / " : "") + error;
    }
  }
  if (pollWorkerReady && options.progress && options.url.length() && WiFi.status() == WL_CONNECTED &&
      monoPoll::due(millis(), lastRequest, lastFailure, pollIntervalMs(), retryPending, requestInFlight)) {
    Job job{}; options.url.toCharArray(job.url, sizeof(job.url)); job.revision = revision;
    if (xQueueOverwrite(jobs, &job) == pdTRUE) {
      lastRequest = millis(); requestWasRetry = retryPending; requestInFlight = true;
    }
  }
  if (bootUntil && (int32_t)(millis() - bootUntil) < 0) {
    boot("READY / LOCAL", "http://" + WiFi.localIP().toString(), "Local management / no login", 100); return;
  }
  bootUntil = 0;
  if (WiFi.status() != WL_CONNECTED) { boot("CONNECT", "", "", 40); return; }
  if (reminderActive) {
    if (millis() - reminderStarted < 3000) { drawReminder(); return; }
    reminderActive = false; legacyNeedsRefresh = !options.progress;
  }
  static uint32_t drawn = 0;
  if (options.progress && millis() - drawn >= 250) { drawProgress(); drawn = millis(); }
}
} // namespace mono
