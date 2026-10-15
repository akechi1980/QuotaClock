"""Read-only unofficial dashboard adapters. Never log credentials or response bodies."""
import json
import math
import threading
import time
from datetime import datetime, timedelta, timezone
from pathlib import Path
from urllib.request import Request, build_opener, HTTPRedirectHandler

BEIJING = timezone(timedelta(hours=8))


def refresh_interval(now=None):
    """Successful upstream refreshes: 10:00..03:00 UTC+8 fast, otherwise slow."""
    now = datetime.now(BEIJING) if now is None else now
    if now.tzinfo is None:
        raise ValueError('refresh time must be timezone-aware')
    hour = now.astimezone(BEIJING).hour
    return 120 if hour >= 10 or hour < 3 else 300


def refresh_delay(now=None):
    """Wake at a schedule boundary rather than carrying a five-minute sleep past it."""
    now = datetime.now(BEIJING) if now is None else now
    if now.tzinfo is None:
        raise ValueError('refresh time must be timezone-aware')
    now = now.astimezone(BEIJING)
    if now.hour < 3:
        boundary = now.replace(hour=3, minute=0, second=0, microsecond=0)
    elif now.hour < 10:
        boundary = now.replace(hour=10, minute=0, second=0, microsecond=0)
    else:
        boundary = (now + timedelta(days=1)).replace(hour=3, minute=0, second=0, microsecond=0)
    return min(refresh_interval(now), (boundary - now).total_seconds())


class NoRedirect(HTTPRedirectHandler):
    def redirect_request(self, req, fp, code, msg, headers, newurl):
        return None


def percent(value):
    if type(value) not in (int, float) or not math.isfinite(value) or value < 0:
        raise ValueError('invalid percentage')
    return min(100, int(value + 0.5))


def codex_values(document):
    limits = document['rate_limit']
    windows = [limits['primary_window'], limits['secondary_window']]
    by_duration = {w['limit_window_seconds']: w for w in windows}
    return [percent(by_duration[seconds]['used_percent']) for seconds in (18000, 604800)]


def cursor_value(document):
    # User-selected monthly Auto pool; not on-demand spend or an average of pools.
    return percent(document['individualUsage']['plan']['autoPercentUsed'])


def get_json(url, headers):
    opener = build_opener(NoRedirect())
    with opener.open(Request(url, headers=headers), timeout=10) as response:
        raw = response.read(1048577)
        if len(raw) > 1048576:
            raise ValueError('response too large')
        return json.loads(raw)


class QuotaSource:
    def __init__(self, config):
        self.config = Path(config)
        self.lock = threading.Lock()
        self.cached = None
        self.failed = False
        self.updated = 0
        self.worker = threading.Thread(target=self._run, daemon=True)
        self.worker.start()

    def read(self):
        with self.lock:
            if self.cached is None or self.failed or time.monotonic() - self.updated > 600:
                raise ValueError('quota unavailable')
            return list(self.cached)

    def _run(self):
        while True:
            try:
                config = json.loads(self.config.read_text(encoding='utf-8-sig'))
                openai, cursor = config['openai'], config['cursor']
                token, session = openai['access_token'].strip(), cursor['session_token'].strip()
                if not token or not session:
                    raise ValueError('tokens required')
                headers = {'Authorization': 'Bearer ' + token, 'Accept': 'application/json'}
                if openai.get('account_id'):
                    headers['ChatGPT-Account-Id'] = openai['account_id']
                first = get_json('https://chatgpt.com/backend-api/wham/usage', headers)
                second = get_json('https://cursor.com/api/usage-summary', {
                    'Cookie': 'WorkosCursorSessionToken=' + session,
                    'Accept': 'application/json',
                })
                values = codex_values(first) + [cursor_value(second)]
            except Exception:
                # Keep old values internally, but fail fast so ROM ages its data.
                with self.lock:
                    self.failed = True
                time.sleep(60)
            else:
                with self.lock:
                    self.cached = values
                    self.failed = False
                    self.updated = time.monotonic()
                time.sleep(refresh_delay())
