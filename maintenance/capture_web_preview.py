"""Capture firmware-sourced mock HTML, never a live device or private account page."""
from datetime import datetime
from functools import partial
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
import subprocess
import sys
import tempfile
import threading
from PIL import Image

ROOT = Path(__file__).resolve().parent.parent


def main():
    subprocess.run([sys.executable, str(ROOT / 'maintenance/update_web_preview.py')], check=True)
    site = ROOT / 'github_submit' if (ROOT / 'github_submit').is_dir() else ROOT
    browser = next((p for p in (Path('C:/Program Files/Google/Chrome/Application/chrome.exe'),
                               Path('C:/Program Files (x86)/Microsoft/Edge/Application/msedge.exe')) if p.exists()), None)
    if browser is None:
        raise RuntimeError('Chrome or Edge required for local screenshot generation')
    output = site / 'docs/images'
    output.mkdir(parents=True, exist_ok=True)
    profile = Path(tempfile.mkdtemp(prefix='quotaclock-preview-'))
    handler = partial(SimpleHTTPRequestHandler, directory=str(site))
    server = ThreadingHTTPServer(('127.0.0.1', 0), handler)
    threading.Thread(target=server.serve_forever, daemon=True).start()
    try:
        url = f'http://127.0.0.1:{server.server_port}/clock_rom/hourly_web_preview.html?mode=weather'
        full = output / 'web-management-full.png'
        result = subprocess.run([str(browser), '--headless=new', '--disable-gpu', '--no-first-run',
                                 '--no-default-browser-check', '--hide-scrollbars', '--user-data-dir=' + str(profile),
                                 '--window-size=1170,2000', '--virtual-time-budget=2000', '--screenshot=' + str(full), url],
                                capture_output=True, timeout=30)
        if result.returncode:
            raise RuntimeError('Local browser capture failed; no private response was requested')
        image = Image.open(full)
        image.crop((160,150,1010,1042)).save(output / 'web-management.png')
        image.crop((588,315,1010,682)).save(output / 'weather-switch.png')
        print('Captured firmware HTML with offline example data. Inspect crops after any layout change.')
    finally:
        server.shutdown()
        server.server_close()


if __name__ == '__main__':
    main()
