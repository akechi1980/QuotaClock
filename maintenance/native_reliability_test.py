"""Execute production C++ parser/transport decisions using MSVC + host adapters.

No device/network access. Keeps timestamped compiler output and test artifacts.
The socket/time adapters are simulations, not tests of ESP32's Wi-Fi driver.
"""
from datetime import datetime
import hashlib
import json
from pathlib import Path
import subprocess

ROOT = Path(__file__).resolve().parent.parent


def main():
    vswhere = Path(r'C:\Program Files (x86)\Microsoft Visual Studio\Installer\vswhere.exe')
    installation = subprocess.check_output([str(vswhere), '-latest', '-products', '*', '-requires',
        'Microsoft.VisualStudio.Component.VC.Tools.x86.x64', '-property', 'installationPath'], text=True).strip()
    vcvars = Path(installation) / 'VC/Auxiliary/Build/vcvars64.bat'
    assert vcvars.is_file(), 'MSVC C++ Build Tools required'
    output = ROOT / 'build' / ('native_reliability_' + datetime.now().strftime('%Y%m%d_%H%M%S_%f'))
    output.mkdir(parents=True, exist_ok=False)
    weather = (ROOT / 'clock_rom/weather_runtime.h').read_text(encoding='utf-8')
    snapshot = weather.split('struct Snapshot {', 1)[1].split('\n', 1)[0]
    parser = weather.split('bool assignedObject(', 1)[1].split('class Body :', 1)[0]
    code = r'''
#define ARDUINOJSON_ENABLE_ARDUINO_STRING 1
#define ARDUINOJSON_ENABLE_ARDUINO_STREAM 0
#define ARDUINOJSON_ENABLE_ARDUINO_PRINT 0
#include "Arduino.h"
#include <ArduinoJson.h>
#include "bounded_client.h"
#include <iostream>
#include <stdexcept>
uint32_t fakeMillis = 0;
FakeWiFi WiFi;
bool fakeSendBlocked = false;
unsigned selfTestsPassed = 0;
'''
    code += 'struct Snapshot {' + snapshot + '\nbool assignedObject(' + parser
    code += r'''
void check(bool pass, const char* message) { if (!pass) throw std::runtime_error(message); }
int main() {
  check(selfTest() && selfTestsPassed == 8, "Production weather parser fixtures");
  uint8_t bytes[32768] = {};
  IPAddress ip;
  BoundedClient normal(10000);
  check(normal.connect(ip, 80) && normal.connectTimeout == 3000, "Connection timeout cap");
  check(normal.write(bytes, 32) == 32, "Normal nonblocking send");
  fakeMillis = 9999;
  check(normal.connected(), "Before deadline");
  fakeMillis = 10000;
  check(!normal.connected() && normal.read() == -1, "Slow header/read is closed at deadline");
  fakeMillis = 0xfffffff0;
  BoundedClient rollover(100);
  check(rollover.connect(ip, 80), "Rollover connect");
  fakeMillis += 100;
  check(!rollover.connected(), "Deadline across millis rollover");
  fakeMillis = 0;
  BoundedClient oversized(10000); oversized.connect(ip, 80);
  check(oversized.read(bytes, sizeof(bytes)) == 32768 && oversized.available() == 0 && !oversized.connected(), "Header plus body byte cap");
  fakeMillis = 0;
  BoundedClient blocked(100); blocked.connect(ip, 80); fakeSendBlocked = true;
  check(blocked.write(bytes, 1) == 0 && fakeMillis == 100 && !blocked.connected(), "Blocked send yields and expires");
  fakeSendBlocked = false;
  fakeMillis = 0; WiFi.dnsDelay = 15000;
  BoundedClient dns(10000);
  check(dns.connect("weather.example", 80) && fakeMillis == 15000, "DNS is separate from transport budget");
  fakeMillis += 10000;
  check(!dns.connected(), "Transport deadline after DNS");
  WiFi.dnsOK = false;
  BoundedClient failedDNS(10000);
  check(!failedDNS.connect("weather.example", 80), "DNS failure returns");
  std::cout << "PASS: 8 production parser fixtures; 11 transport/deadline assertions (simulated socket/time)\n";
}
'''
    (output / 'test.cpp').write_text(code, encoding='utf-8')
    includes = [ROOT / 'maintenance/native_stubs', ROOT / 'clock_rom', ROOT / 'vendor/clock_rom/libraries/ArduinoJson/src']
    command = ['cl', '/nologo', '/EHsc', '/std:c++14', '/utf-8', '/W3']
    command += [f'/I"{p}"' for p in includes]
    command += [f'/Fe:"{output / "test.exe"}"', f'/Fo:"{output / "test.obj"}"', f'"{output / "test.cpp"}"']
    script = '@echo off\ncall "' + str(vcvars) + '" >nul\nif errorlevel 1 exit /b 1\n' + ' '.join(command) + '\nexit /b %errorlevel%\n'
    (output / 'compile.cmd').write_text(script, encoding='utf-8')
    result = subprocess.run(['cmd', '/d', '/c', str(output / 'compile.cmd')], capture_output=True)
    (output / 'compile.log').write_bytes(result.stdout + result.stderr)
    if result.returncode:
        print(result.stdout.decode('mbcs', 'replace')[-8000:].encode('ascii', 'backslashreplace').decode())
        raise SystemExit(result.returncode)
    run = subprocess.run([str(output / 'test.exe')], capture_output=True, timeout=20)
    (output / 'test.log').write_bytes(run.stdout + run.stderr)
    print(run.stdout.decode('utf-8', 'replace'))
    assert run.returncode == 0, str(output / 'test.log')
    report = dict(scope='production C++ with simulated socket/time/String adapters, not hardware',
        files={name: hashlib.sha256((ROOT / 'clock_rom' / name).read_bytes()).hexdigest()
               for name in ('weather_runtime.h', 'bounded_client.h')}, passed=True)
    (output / 'result.json').write_text(json.dumps(report, indent=2), encoding='utf-8')
    print('Native evidence:', output)


if __name__ == '__main__':
    main()
