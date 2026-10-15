"""Local UI mock only. Does not connect to an ESP32 or change saved ROM settings."""
import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import time

ROOT = Path(__file__).resolve().parent


def create_server(port=8767):
    settings = dict(brightness=50, rotation=0, url='', interval_minutes=5, mode='progress', city='101280601', weather_interval=10)
    started = time.monotonic()

    class Handler(BaseHTTPRequestHandler):
        def reply(self, status, body, kind='application/json'):
            data = json.dumps(body).encode('utf-8') if kind == 'application/json' else body
            self.send_response(status)
            self.send_header('Content-Type', kind)
            self.send_header('Content-Length', str(len(data)))
            self.send_header('Cache-Control', 'no-store')
            self.end_headers(); self.wfile.write(data)

        def do_GET(self):
            if self.path in ('/', '/device'):
                page = 'web_page.html' if self.path == '/' else 'device_preview.html'
                self.reply(200, (ROOT / page).read_bytes(), 'text/html; charset=utf-8')
            elif self.path == '/api/status':
                step = int((time.monotonic() - started) / 1.2)
                self.reply(200, dict(settings, version='DESIGN PREVIEW — NOT DEVICE', connected=True,
                                     ip='127.0.0.1', csrf='preview-only', source_state='simulated design data',
                                     tasks=[dict(label=name, percent=(step+i*33)%101, stale=False) for i,name in enumerate(['OpenAI 5 Hours','OpenAI Week','Cursor Month'])]))
            else:
                self.reply(404, {'error': 'not found'})

        def do_POST(self):
            if self.headers.get('X-CP-Token') != 'preview-only':
                self.reply(403, {'error': 'invalid preview token'}); return
            if self.path == '/api/portal':
                self.reply(200, {'ok': True, 'note': 'mock only; no access point started'}); return
            if self.path != '/api/config':
                self.reply(404, {'error': 'not found'}); return
            try:
                length = int(self.headers.get('Content-Length', '0'))
                if not 0 < length <= 1024:
                    raise ValueError()
                data = json.loads(self.rfile.read(length))
                settings.update({key: data[key] for key in settings})
                self.reply(200, {'ok': True, 'note': 'preview memory only'})
            except (ValueError, KeyError, TypeError):
                self.reply(400, {'error': 'invalid preview request'})

        def log_message(self, *args):
            pass

    return ThreadingHTTPServer(('127.0.0.1', port), Handler)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--port', type=int, default=8767)
    args = parser.parse_args()
    server = create_server(args.port)
    print(f'Design mock: http://127.0.0.1:{server.server_port}/ and /device', flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()
