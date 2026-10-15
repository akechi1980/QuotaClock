"""Create an isolated V1.4 development copy/toolchain; never touches ROM or originals."""
from pathlib import Path
import hashlib
import json
import shutil
import subprocess

ROOT = Path(__file__).resolve().parent
WORK = ROOT.parent
ORIGINAL = WORK / 'vendor/clock_v14_s3/code'
ENV = WORK / 'vendor/clock_rom'
CLI = Path(r'C:\Program Files\Arduino CLI\arduino-cli.exe')
GLOBAL = Path.home() / 'AppData/Local/Arduino15'


def copy_once(source, destination):
    destination.parent.mkdir(parents=True, exist_ok=True)
    if not destination.exists():
        shutil.copyfile(source, destination)


def prepare():
    # Preserve author's attribution and assets in an independent sketch copy.
    source = ORIGINAL / 'SmallDesktopDisplay'
    if not (ROOT / 'clock_rom.ino').exists():
        for file in source.rglob('*'):
            if file.is_file():
                relative = file.relative_to(source)
                if file.suffix.lower() == '.bin':
                    # The sketch archive includes a stale C3 bootloader. The
                    # verified S3 flash package is separate; never auto-use this.
                    continue
                if relative.name == 'SmallDesktopDisplay.ino':
                    relative = Path('clock_rom.ino')
                copy_once(file, ROOT / relative)
    libraries = ENV / 'libraries'
    if not libraries.exists():
        # Windows resolves Time.h as time.h, masking the C runtime header.
        # Time.cpp uses TimeLib.h; omit only this deprecated forwarding header
        # in the development COPY. The original library remains unchanged.
        shutil.copytree(ORIGINAL / 'libraries', libraries,
                        ignore=lambda directory, names: ['Time.h']
                        if Path(directory).name == 'Time-Library' else [])
    data = ENV / 'data'
    downloads = ENV / 'downloads'
    data.mkdir(parents=True, exist_ok=True)
    downloads.mkdir(parents=True, exist_ok=True)
    for file in GLOBAL.glob('package*'):
        if file.is_file():
            copy_once(file, data / file.name)
    packages = []
    for file in GLOBAL.glob('package*_index.json'):
        packages.extend(json.loads(file.read_text())['packages'])
    esp = next(p for p in packages if p['name'] == 'esp32')
    core = next(p for p in esp['platforms'] if p['version'] == '2.0.12')
    archives = [core]
    for dep in core['toolsDependencies']:
        provider = next(p for p in packages if p['name'] == dep['packager'])
        tool = next(t for t in provider['tools'] if t['name'] == dep['name']
                    and t['version'] == dep['version'])
        candidates = [s for s in tool['systems'] if 'mingw32' in s['host']]
        chosen = next((s for s in candidates if 'x86_64' in s['host']), candidates[0])
        archives.append(chosen)
    for archive in archives:
        cached = GLOBAL / 'staging/packages' / archive['archiveFileName']
        if cached.exists():
            expected = archive['checksum'].split(':', 1)[1].lower()
            if hashlib.sha256(cached.read_bytes()).hexdigest() != expected:
                raise ValueError('Cache checksum failed: ' + cached.name)
            copy_once(cached, downloads / 'packages' / cached.name)
    config = ENV / 'arduino-cli.yaml'
    if not config.exists():
        config.write_text('directories:\n  data: ' + json.dumps(str(data)) +
                          '\n  downloads: ' + json.dumps(str(downloads)) +
                          '\n  user: ' + json.dumps(str(ENV / 'user')) +
                          '\nboard_manager:\n  additional_urls:\n' +
                          '    - https://espressif.github.io/arduino-esp32/package_esp32_index.json\n',
                          encoding='utf-8')
    subprocess.run([str(CLI), '--config-file', str(config), 'core', 'install',
                    'esp32:esp32@2.0.12'], check=True)
    print('Prepared isolated core 2.0.12; global core and verified originals untouched.')


if __name__ == '__main__':
    prepare()
