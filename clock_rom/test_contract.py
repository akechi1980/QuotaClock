"""Offline source/API contract checks, not hardware or Wi-Fi integration tests."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parent


class ContractTests(unittest.TestCase):
    def test_wifi_recovery_uses_saved_config_not_connected_ssid(self):
        source = (ROOT / 'mono_integration.h').read_text(encoding='utf-8')
        sketch = (ROOT / 'clock_rom.ino').read_text(encoding='utf-8')
        rules = (ROOT / 'wifi_recovery.h').read_text()
        self.assertIn('esp_wifi_get_config(WIFI_IF_STA, &config)', source)
        self.assertNotIn('WiFi.SSID()', sketch.split('void setup()')[1].split('#if 0')[0])
        self.assertIn('mono::connectSavedWifi()', sketch)
        self.assertIn('startPortal(true)', source)
        self.assertIn('wm.stopConfigPortal()', source)
        self.assertIn('WiFi.softAPgetStationNum()', source)
        self.assertIn('if (!routesRegistered)', source)
        for guard in ('Unknown config is not empty config', 'Manual reconfiguration stays open',
                      'Do not interrupt a phone', 'Connected device never falls back for quota failure'):
            self.assertIn(guard, rules)

    def test_grey_page_no_external_framework(self):
        page = (ROOT / 'web_page.html').read_text(encoding='utf-8')
        self.assertLess(len(page.encode('utf-8')), 16000)
        self.assertNotIn('<script src=', page)
        self.assertNotIn('@import', page)
        self.assertIn('--bg:#202427', page)
        self.assertIn("'X-CP-Token':token", page)

    def test_no_auto_upload(self):
        source = (ROOT / 'build.ps1').read_text(encoding='utf-8')
        self.assertNotIn('--upload', source)
        self.assertNotIn('--port', source)
        self.assertIn('verify.py', source)
        self.assertIn("'run_'", source)
        self.assertIn('--build-dir', source)
        self.assertIn("--build-property 'build.partitions=huge_app'", source)
        partitions = (ROOT / 'partitions.csv').read_text()
        self.assertIn('0x10000, 0x300000', partitions)
        self.assertNotIn('app1', partitions)

    def test_async_bounded_progress_response(self):
        source = (ROOT / 'mono_integration.h').read_text(encoding='utf-8')
        self.assertIn('xTaskCreatePinnedToCore', source)
        self.assertIn('body.length() + count > 2048', source)
        self.assertIn('array.size() == 3', source)
        self.assertIn('result.revision == revision', source)
        self.assertNotIn('Update.begin(', source)

    def test_no_stale_blink_and_mock_labeling(self):
        source = (ROOT / 'mono_integration.h').read_text(encoding='utf-8')
        self.assertIn('demo ? "DEMO / " : simulated ? "SIM / " : "LIVE / "', source)
        for name in ['OpenAI 5 Hours', 'OpenAI Week', 'Cursor Month']:
            self.assertIn(name, source)
        self.assertIn('stale ? "%u%%*" : "%u%%"', source)
        self.assertNotIn('kBlinkMs', source)
        self.assertIn('taskStep = 42', source)
        self.assertIn('footerTop = 218', source)

    def test_no_sketch_bootloader_override(self):
        self.assertFalse((ROOT / 'bootloader.bin').exists())

    def test_existing_chinese_calendar_font(self):
        import re
        import struct
        source = (ROOT / 'mono_integration.h').read_text(encoding='utf-8')
        self.assertIn('calendar.loadFont(ZdyLwFont_20)', source)
        self.assertIn('"星期四"', source)
        self.assertIn('rightAligned ? 224 - targetWidth : 16', source)
        self.assertIn('calendarText(text, false, 9)', source)
        self.assertIn('calendarText(synced ? days[local.tm_wday] : "星期--", true, 10)', source)
        raw = (ROOT / 'font/ZdyLwFont_20.h').read_text()
        data = bytes(int(value, 16) for value in re.findall(r'0x([0-9a-fA-F]{2})', raw))
        count = struct.unpack_from('>I', data)[0]
        advances = {struct.unpack_from('>I', data, 24+i*28)[0]: struct.unpack_from('>I', data, 24+i*28+12)[0] for i in range(count)}
        self.assertTrue(all(ord(c) in advances for c in '年月日星期一二三四五六0123456789'))
        self.assertLessEqual(sum(advances[ord(c)] for c in '2026年12月31日') * .9 + 60, 208)
        self.assertLessEqual(sum(advances[ord(c)] for c in '12月31日星期四'), 208)

    def test_requested_no_login_and_clean_weather_labels(self):
        page = (ROOT / 'web_page.html').read_text(encoding='utf-8')
        source = (ROOT / 'mono_integration.h').read_text(encoding='utf-8')
        self.assertNotIn('server.authenticate(', source)
        self.assertNotIn('requestAuthentication(', source)
        self.assertNotIn('保留的 V1.4 天气设置', page)
        self.assertNotIn('V1.4 原天气界面', page)
        self.assertIn('天气时钟', page)
        self.assertIn('管理网页免登录', page)
        self.assertNotIn('id="weatherSettings" hidden', page)
        self.assertIn("el('weatherSettings').hidden=false", page)

    def test_numeric_ap_password(self):
        source = (ROOT / 'mono_integration.h').read_text(encoding='utf-8')
        self.assertIn('key.length() != 8', source)
        self.assertIn('"%08lu"', source)
        self.assertIn('p.putString("access", accessKey)', source)
        self.assertIn('frame.drawString(accessKey, 18, 126, 4)', source)
        self.assertIn('{"wifinoscan", "exit"}', source)
        self.assertIn('millis() - lastPortalDraw >= 500', source)

    def test_hourly_settings_audio_and_time_size(self):
        source = (ROOT / 'mono_integration.h').read_text(encoding='utf-8')
        audio = (ROOT / 'mono_audio.h').read_text(encoding='utf-8')
        page = (ROOT / 'web_page.html').read_text(encoding='utf-8')
        self.assertIn('bool hourly = false; uint8_t volume = 30', source)
        self.assertIn('doc["hourly_enabled"] = value.hourly', source)
        self.assertIn('doc["hourly_volume"] = value.volume', source)
        self.assertIn('monoHourly::due(epoch, lastClock, highestHour', source)
        self.assertIn('millis() - reminderStarted < 3000', source)
        self.assertIn('frame.drawCentreString(text, 120, 43, synced ? 7 : 4)', source)
        self.assertIn('frame.drawCentreString(text, 120, 82, 7)', source)
        self.assertIn('"/api/reminder/test", HTTP_POST', source)
        self.assertIn('xQueueSend(requests, &job, 0)', audio)
        self.assertIn('digitalWrite(pa, LOW)', audio)
        self.assertIn('sda = 40, scl = 39, mclk = 38, bclk = 16, ws = 7, dout = 4, pa = 8', audio)
        self.assertIn("request('/api/reminder/test'", page)
        self.assertNotIn('delay(', audio)

    def test_five_second_hourly_schedule_guards_clock_jumps(self):
        schedule = (ROOT / 'hourly_schedule.h').read_text()
        self.assertIn('windowSeconds = 5', schedule)
        for rule in ('No startup chime', 'Never duplicate an hour', 'Reject backward clock movement',
                     'Reject fast wall-clock correction', 'Tolerate a five-second loop delay'):
            self.assertIn(rule, schedule)

    def test_configurable_beeps_and_partial_toggle(self):
        source = (ROOT / 'mono_integration.h').read_text(encoding='utf-8')
        page = (ROOT / 'web_page.html').read_text(encoding='utf-8')
        audio = (ROOT / 'mono_audio.h').read_text()
        self.assertIn('uint8_t beeps = 3', source)
        self.assertIn('doc["hourly_beeps"] = value.beeps', source)
        self.assertIn('doc.size() != 1', source)
        self.assertIn('Options candidate = options; candidate.hourly = enabled', source)
        self.assertIn('"/api/reminder/settings", HTTP_POST, configureReminder', source)
        self.assertIn('monoAudio::request(options.volume, options.beeps)', source)
        self.assertIn('sampleRate * beeps / 2', audio)
        self.assertIn('beeps < 1 || beeps > 5', audio)
        self.assertIn('hourly-toggle::before', page)
        self.assertIn("request('/api/reminder/settings',{hourly_enabled:on})", page)
        self.assertIn("renderHourly(previous,", page)
        self.assertIn("beeps:Number(el('hourlyBeeps').value)", page)

    def test_one_retry_preserves_health_until_second_failure(self):
        source = (ROOT / 'mono_integration.h').read_text(encoding='utf-8')
        schedule = (ROOT / 'poll_schedule.h').read_text(encoding='utf-8')
        self.assertIn('timeoutMs = 3000', schedule)
        self.assertIn('retryMs = 10000', schedule)
        self.assertIn('return !success && !wasRetry', schedule)
        self.assertIn('wasRetry ? false : previousHealthy', schedule)
        self.assertIn('No repeated ten-second retry loop', schedule)
        self.assertIn('http.setConnectTimeout(monoPoll::timeoutMs)', source)
        self.assertIn('http.setTimeout(monoPoll::timeoutMs)', source)
        self.assertIn('monoPoll::healthyAfterResult(result.valid, requestWasRetry, pollHealthy)', source)
        self.assertIn('monoPoll::retryAfterResult(result.valid, requestWasRetry)', source)
        self.assertIn('requestWasRetry = retryPending', source)
        self.assertNotIn('pollHealthy = result.valid', source)


if __name__ == '__main__':
    unittest.main()
