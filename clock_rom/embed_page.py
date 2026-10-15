from pathlib import Path
ROOT = Path(__file__).resolve().parent
page = (ROOT / 'web_page.html').read_text(encoding='utf-8')
if ')MONOPAGE"' in page:
    raise ValueError('Invalid raw-string delimiter in page')
(ROOT / 'management_page.h').write_text(
    '// Generated from web_page.html; no external assets.\n#pragma once\n'
    'static const char managementPage[] PROGMEM = R"MONOPAGE(' + page + ')MONOPAGE";\n', encoding='utf-8')
print('Embedded management page:', len(page.encode('utf-8')), 'bytes')
