import unittest
from unittest.mock import patch
import time
from datetime import datetime, timezone
from quota import BEIJING, QuotaSource, codex_values, cursor_value, percent, refresh_interval, refresh_delay


class QuotaTests(unittest.TestCase):
    def test_refresh_time_windows(self):
        for hour, minute, second, expected in [
            (0, 0, 0, 120), (2, 59, 59, 120), (3, 0, 0, 300),
            (9, 59, 59, 300), (10, 0, 0, 120), (23, 59, 59, 120),
        ]:
            with self.subTest(hour=hour, minute=minute, second=second):
                now = datetime(2026, 10, 15, hour, minute, second, tzinfo=BEIJING)
                self.assertEqual(refresh_interval(now), expected)

    def test_refresh_boundary_wakeup(self):
        for hour, minute, second, expected in [
            (2, 59, 59, 1), (3, 0, 0, 300), (9, 59, 30, 30),
            (10, 0, 0, 120), (23, 59, 59, 120),
        ]:
            now = datetime(2026, 10, 15, hour, minute, second, tzinfo=BEIJING)
            self.assertEqual(refresh_delay(now), expected)

    def test_schedule_uses_beijing_not_machine_timezone(self):
        self.assertEqual(refresh_interval(datetime(2026, 10, 15, 2, 0, tzinfo=timezone.utc)), 120)
        self.assertEqual(refresh_interval(datetime(2026, 10, 15, 19, 0, tzinfo=timezone.utc)), 300)
        with self.assertRaises(ValueError):
            refresh_interval(datetime(2026, 10, 15, 10, 0))

    def test_worker_uses_scheduled_success_delay(self):
        usage = {'rate_limit': {
            'primary_window': {'limit_window_seconds': 18000, 'used_percent': 5},
            'secondary_window': {'limit_window_seconds': 604800, 'used_percent': 54},
        }}
        with patch('quota.threading.Thread.start'):
            source = QuotaSource('unused')
        config = '{"openai":{"access_token":"example"},"cursor":{"session_token":"example"}}'
        with patch('quota.Path.read_text', return_value=config), \
                patch('quota.get_json', side_effect=[usage, {'individualUsage': {'plan': {'autoPercentUsed': 18}}}]), \
                patch('quota.refresh_delay', return_value=120), \
                patch('quota.time.sleep', side_effect=StopIteration) as sleep:
            with self.assertRaises(StopIteration):
                source._run()
            sleep.assert_called_once_with(120)
            self.assertEqual(source.read(), [5, 54, 18])

    def test_failure_keeps_existing_one_minute_retry(self):
        with patch('quota.threading.Thread.start'):
            source = QuotaSource('unused')
        with patch('quota.Path.read_text', side_effect=OSError), \
                patch('quota.time.sleep', side_effect=StopIteration) as sleep:
            with self.assertRaises(StopIteration):
                source._run()
            sleep.assert_called_once_with(60)
            self.assertTrue(source.failed)

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
