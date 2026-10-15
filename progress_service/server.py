"""Small read-only JSON progress service; stdlib only, no installer/firewall changes."""
import argparse
from http.server import BaseHTTPRequestHandler, ThreadingHTTPServer
import json
from pathlib import Path
import time
from urllib.parse import urlsplit


def validate(values):
    if not isinstance(values, list) or len(values) != 3:
        raise ValueError('progress must contain exactly three values')
    if any(type(v) is not int or not 0 <= v <= 100 for v in values):
        raise ValueError('each progress value must be an integer in 0..100')
    return list(values)


def create_server(host='127.0.0.1', port=8766, values=None, file=None, demo=False, config=None):
    if config:
        from quota import QuotaSource
        quota = QuotaSource(config)
    fixed = validate([25, 50, 75] if values is None else values)
    started = time.monotonic()

    class Handler(BaseHTTPRequestHandler):
        def do_GET(self):
            path = urlsplit(self.path).path
            code = 200
            if path == '/health':
                data = {'ok': True}
            elif path == '/api/progress':
                try:
                    if config:
                        current = quota.read()
                    elif file:
                        raw = Path(file).read_bytes()
                        if len(raw) > 2048:
                            raise ValueError('progress file exceeds 2048 bytes')
                        document = json.loads(raw)
                        current = validate(document['progress'])
                    elif demo:
                        step = int((time.monotonic() - started) / 1.2)
                        current = [(step + i * 33) % 101 for i in range(3)]
                    else:
                        current = fixed
                    data = {'progress': current, 'simulation': not bool(file or config)}
                except (OSError, ValueError, KeyError, TypeError):
                    code, data = 503, {'error': 'progress source unavailable or invalid'}
            else:
                code, data = 404, {'error': 'not found'}
            body = json.dumps(data, separators=(',', ':')).encode('utf-8')
            self.send_response(code)
            self.send_header('Content-Type', 'application/json; charset=utf-8')
            self.send_header('Cache-Control', 'no-store')
            self.send_header('Content-Length', str(len(body)))
            self.end_headers()
            self.wfile.write(body)

        def log_message(self, format, *args):
            pass  # Do not record URLs/query strings containing incidental credentials.

    return ThreadingHTTPServer((host, port), Handler)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--host', default='127.0.0.1', help='Use 0.0.0.0 explicitly for LAN access')
    parser.add_argument('--port', type=int, default=8766)
    source = parser.add_mutually_exclusive_group()
    source.add_argument('--values', nargs=3, type=int)
    source.add_argument('--file', type=Path, help='Read {"progress":[...]} from this file each request')
    source.add_argument('--demo', action='store_true', help='Explicitly simulated slow looping values')
    source.add_argument('--config', type=Path, help='Private provider-token JSON configuration')
    args = parser.parse_args()
    if not 0 < args.port < 65536:
        parser.error('port must be 1..65535')
    server = create_server(args.host, args.port, args.values, args.file, args.demo, args.config)
    print(f'Listening http://{args.host}:{args.port}/api/progress', flush=True)
    print('Read-only test service; expose only to a trusted LAN. No firewall rules changed.', flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        pass
    finally:
        server.server_close()


if __name__ == '__main__':
    main()
