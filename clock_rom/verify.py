"""Offline verification/8MB packaging only. No serial/device access."""
from datetime import datetime
import hashlib
import io
import json
import re
from pathlib import Path
import struct
import argparse
from esptool.bin_image import ESP32S3FirmwareImage

ROOT = Path(__file__).resolve().parent
BUILD = ROOT.parent / 'build/clock_rom'
FLASH = 8388608


def verify():
    sha = lambda data: hashlib.sha256(data).hexdigest()
    options = json.loads((BUILD / 'build.options.json').read_text())
    if not all(v in options['fqbn'] for v in ['esp32:esp32:esp32s3:', 'FlashSize=8M', 'PSRAM=opi', 'PartitionScheme=huge_app']):
        raise ValueError('Unexpected target')
    artifacts = {}
    for suffix in ['bootloader', 'partitions', 'app']:
        name = 'clock_rom.ino.bin' if suffix == 'app' else f'clock_rom.ino.{suffix}.bin'
        artifacts[suffix] = (BUILD / name).read_bytes()
    for name in ['bootloader', 'app']:
        image = ESP32S3FirmwareImage(io.BytesIO(artifacts[name]))
        image.verify()
        if image.checksum != image.calculate_checksum() or image.stored_digest != image.calc_digest:
            raise ValueError('Checksum/digest failure: ' + name)
        if struct.unpack_from('<H', artifacts[name], 12)[0] != 9:
            raise ValueError('Wrong chip: ' + name)
    if artifacts['bootloader'][3] >> 4 != 3:
        raise ValueError('Bootloader does not advertise 8MB')
    table = artifacts['partitions']; entries = []; checked = False
    if sha(table) != 'aaae2888c5a6a348004b5b436f47abb25ae32e72d9003902955a998eda723edd':
        raise ValueError('Partition layout differs from the verified original huge_app baseline')
    for start in range(0, len(table), 32):
        entry = table[start:start + 32]
        if entry[:2] == b'\xeb\xeb':
            checked = hashlib.md5(table[:start]).digest() == entry[16:32]
            break
        if entry[:2] != b'\xaa\x50':
            break
        _, kind, subtype, offset, size, label, flags = struct.unpack('<HBBII16sI', entry)
        if size == 0 or offset < 0x9000 or offset + size > FLASH:
            raise ValueError('Invalid partition boundary')
        entries.append(dict(name=label.split(b'\0')[0].decode(), type=kind, offset=offset, size=size))
    if not checked:
        raise ValueError('Partition MD5 failed')
    ordered = sorted(entries, key=lambda item: item['offset'])
    if any(a['offset'] + a['size'] > b['offset'] for a, b in zip(ordered, ordered[1:])):
        raise ValueError('Overlapping partitions')
    app = next(p for p in entries if p['name'] == 'app0')
    if app['offset'] != 0x10000 or len(artifacts['app']) > app['size']:
        raise ValueError('Application exceeds original huge_app slot')
    boot_app = ROOT.parent / 'vendor/clock_rom/data/packages/esp32/hardware/esp32/2.0.12/tools/partitions/boot_app0.bin'
    artifacts['boot_app0'] = boot_app.read_bytes()
    merged = bytearray(b'\xff' * FLASH)
    layout = [(0, 'bootloader'), (0x8000, 'partitions'), (0xe000, 'boot_app0'), (0x10000, 'app')]
    for i, (offset, name) in enumerate(layout):
        data = artifacts[name]
        ceiling = layout[i + 1][0] if i + 1 < len(layout) else app['offset'] + app['size']
        if offset + len(data) > ceiling:
            raise ValueError('Image overlaps next region: ' + name)
        merged[offset:offset + len(data)] = data
    (BUILD / 'clock_rom.ino.merged.bin').write_bytes(merged)
    record = dict(checked_at=datetime.now().astimezone().isoformat(), source_family='SDD V1.4',
                  version=re.search(r'constexpr const char\* version = "([^"]+)"', (ROOT / 'mono_integration.h').read_text(encoding='utf-8')).group(1), isolated_core='2.0.12', fqbn=options['fqbn'],
                  application_size=len(artifacts['app']), merged_size=len(merged), merged_sha256=sha(merged), build_directory=str(BUILD),
                  partition_md5_verified=True, image_checksums_verified=True, partitions=entries,
                  hardware_runtime_tested=False, upload_performed=False,
                  web_page_bytes=(ROOT / 'web_page.html').stat().st_size,
                  artifacts={name: dict(size=len(data), sha256=sha(data)) for name, data in artifacts.items()},
                  source_sha256={p.relative_to(ROOT).as_posix(): sha(p.read_bytes()) for p in sorted(ROOT.rglob('*'))
                                 if p.is_file() and p.suffix in ['.ino', '.h', '.cpp', '.html', '.py', '.ps1']})
    target = BUILD / ('manifest_' + datetime.now().strftime('%Y%m%d_%H%M%S_%f') + '.json')
    with target.open('x', encoding='utf-8') as output:
        json.dump(record, output, ensure_ascii=False, indent=2); output.write('\n')
    print('PASS: S3 images, original huge_app layout, partition MD5, full 8MB image')
    print('Application bytes:', record['application_size'], 'Page bytes:', record['web_page_bytes'])
    print('Build manifest:', target)


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--build-dir', type=Path, default=BUILD)
    BUILD = parser.parse_args().build_dir.resolve()
    verify()
