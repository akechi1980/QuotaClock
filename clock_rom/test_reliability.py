"""Source contracts supplement (not replace) on-device parser/transport checks."""
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parent


class ReliabilityTests(unittest.TestCase):
    def test_loop_has_no_legacy_network_sync(self):
        source = (ROOT / 'clock_rom.ino').read_text(encoding='utf-8')
        loop = source.split('void loop()')[1].split('void LCD_reflash')[0]
        self.assertNotIn('getNtpTime(', loop)
        self.assertNotIn('setSyncProvider(', loop)
        self.assertIn('setTime(systemEpoch + 8 * 3600)', loop)
        self.assertIn('monoWeather::tick', loop)
        self.assertIn('monoWeather::paint()', loop)

    def test_bounded_client_covers_headers_and_write(self):
        source = (ROOT / 'bounded_client.h').read_text()
        for method in ('connected()', 'available()', 'read()', 'peek()'):
            self.assertIn(method + ' override', source)
        self.assertIn('MSG_DONTWAIT', source)
        self.assertIn('byteLimit = 32768', source)
        self.assertIn('(uint32_t)(millis() - started)', source)
        self.assertIn('WiFi.hostByName(host, ip)', source)

    def test_weather_failure_keeps_last_snapshot(self):
        source = (ROOT / 'weather_runtime.h').read_text()
        self.assertIn('result->valid', source)
        self.assertIn('state = "failed_keep_previous"', source)
        self.assertIn('failed ? 60000UL', source)
        self.assertIn('text.length() + n > 24576', source)
        self.assertIn('xQueueCreate(1, sizeof(Snapshot*))', source)
        worker = source.split('void worker(void*)')[1].split('void begin()')[0]
        self.assertNotIn('weaterData(', worker)
        self.assertIn('if (!selfTest())', source)
        self.assertIn('selfTestsPassed', source)

    def test_failed_task_creation_cannot_queue_progress(self):
        source = (ROOT / 'mono_integration.h').read_text(encoding='utf-8')
        self.assertIn('if (pollWorkerReady && options.progress', source)
        self.assertIn('sourceState = "worker overdue"', source)
        self.assertIn('setSaveConnectTimeout(15)', source)
        self.assertIn('"/wifi", HTTP_GET', source)
        self.assertIn('doc["loop_max_ms"]', source)


if __name__ == '__main__':
    unittest.main()
