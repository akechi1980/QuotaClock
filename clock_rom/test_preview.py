"""Offline preview must keep actual firmware UI and never contact a device."""
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parent


class PreviewTests(unittest.TestCase):
    def test_firmware_styles_are_reused(self):
        page = (ROOT / 'web_page.html').read_text(encoding='utf-8')
        preview = (ROOT / 'hourly_web_preview.html').read_text(encoding='utf-8')
        self.assertEqual(re.findall(r'<style>(.*?)</style>', page, re.S),
                         re.findall(r'<style>(.*?)</style>', preview, re.S))

    def test_weather_switch_and_mock_save(self):
        preview = (ROOT / 'hourly_web_preview.html').read_text(encoding='utf-8')
        for token in ('id="mode"', 'value="weather"', 'value="progress"',
                      "path==='/api/config'", 'Object.assign(previewState,body)', '未修改设备'):
            self.assertIn(token, preview)

    def test_api_is_intercepted_before_app(self):
        preview = (ROOT / 'hourly_web_preview.html').read_text(encoding='utf-8')
        self.assertLess(preview.index('window.fetch='), preview.index('async function request('))
        self.assertIn('Offline preview blocks network requests', preview)
        self.assertIn("ip:'192.168.1.50'", preview)


if __name__ == '__main__':
    unittest.main()
