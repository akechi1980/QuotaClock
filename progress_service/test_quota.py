import unittest
from unittest.mock import patch
import time
from quota import QuotaSource, codex_values, cursor_value, percent


class QuotaTests(unittest.TestCase):
    def test_cache_read_never_fetches(self):
        with patch('quota.threading.Thread.start'), patch('quota.get_json') as fetch:
            source = QuotaSource('unused')
            with self.assertRaises(ValueError):
                source.read()
            source.cached = [94, 53, 18]
            source.updated = time.monotonic()
            self.assertEqual(source.read(), [94, 53, 18])
            source.failed = True
            with self.assertRaises(ValueError):
                source.read()
            source.failed = False
            source.updated -= 601
            with self.assertRaises(ValueError):
                source.read()
            fetch.assert_not_called()

    def test_windows_by_duration(self):
        self.assertEqual(codex_values({'rate_limit': {
            'primary_window': {'limit_window_seconds': 604800, 'used_percent': 40},
            'secondary_window': {'limit_window_seconds': 18000, 'used_percent': 12.5},
        }}), [13, 40])

    def test_cursor_auto_only(self):
        self.assertEqual(cursor_value({'individualUsage': {'plan': {
            'totalPercentUsed': 62.2, 'autoPercentUsed': 90,
        }}}), 90)
        with self.assertRaises(KeyError):
            cursor_value({'individualUsage': {'plan': {'totalPercentUsed': 62}}})
        self.assertEqual(cursor_value({'individualUsage': {'plan': {
            'autoPercentUsed': 17.557777777777776,
        }}}), 18)

    def test_invalid(self):
        for value in [True, None, '10', float('nan'), -1]:
            with self.assertRaises(ValueError):
                percent(value)
        self.assertEqual(percent(110), 100)
