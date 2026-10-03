"""Generate embedded TFT_eSPI VLW fonts. Requires Pillow and fonttools."""
from pathlib import Path
import struct
import unicodedata as ud
from PIL import Image, ImageDraw, ImageFont
from fontTools.ttLib import TTFont

ROOT = Path(__file__).resolve().parents[1]
OUT = ROOT / 'include' / 'fonts'
SOURCES = Path(__file__).parent / 'font_sources'
VIETNAMESE = set(map(ord, 'ĂăÂâĐđÊêÔôƠơƯưĨĩŨũ')) | set(range(0x1EA0, 0x1EFA))
CODES = sorted(set(range(0x21, 0x7F)) | set(range(0xA0, 0x100)) | VIETNAMESE
               | set(map(ord, '–—‘’“”…')))

def generate(name, filename, size):
    source = SOURCES / filename
    cmap = TTFont(source).getBestCmap()
    assert set(CODES) <= set(cmap), 'Missing source glyphs'
    font = ImageFont.truetype(str(source), size)
    records, bitmaps = [], []
    ascent = descent = 0
    for code in CODES:
        char = chr(code)
        left, top, right, bottom = font.getbbox(char, anchor='ls')
        width, height = right - left, bottom - top
        advance = round(font.getlength(char))
        image = Image.new('L', (width, height))
        ImageDraw.Draw(image).text((-left, -top), char, font=font, fill=255, anchor='ls')
        records.append(struct.pack('>7i', code, height, width, advance, -top, left, 0))
        bitmaps.append(image.tobytes())
        ascent = max(ascent, -top)
        descent = max(descent, bottom)
        assert width < 256 and height < 256 and advance < 256
    payload = struct.pack('>6i', len(CODES), 11, size, 0, ascent, descent)
    payload += b''.join(records) + b''.join(bitmaps)
    # Validate all offsets against the exact format read by TFT_eSPI 2.2.20.
    offset = 24 + 28 * len(CODES)
    for i, code in enumerate(CODES):
        record = struct.unpack_from('>7i', payload, 24 + 28 * i)
        assert record[0] == code
        offset += record[1] * record[2]
    assert offset == len(payload)
    lines = ['#pragma once', '#include <Arduino.h>',
             '// Generated from DejaVu Sans; license: ../../tools/font_sources/LICENSE_DEJAVU',
             f'// {len(CODES)} Unicode glyphs; {size}px; line height {ascent + descent}px.',
             f'const uint8_t {name}[] PROGMEM = {{']
    lines += ['    ' + ','.join(f'0x{b:02X}' for b in payload[i:i+20]) + ','
              for i in range(0, len(payload), 20)]
    lines += ['};', '']
    (OUT / (name + '.h')).write_text('\n'.join(lines), encoding='utf-8')
    print(f'{name}: {len(CODES)} glyphs, {len(payload)} flash bytes, height {ascent + descent}px')

def normalization_table():
    # Canonical Vietnamese/Latin decompositions (NFD) to precomposed glyphs (NFC).
    records = []
    for code in CODES:
        decomposition = ud.normalize('NFD', chr(code))
        if len(decomposition) in (2, 3) and ord(decomposition[0]) < 128:
            parts = [ord(c) for c in decomposition]
            records.append((parts + [0])[:3] + [code])
    text = ['#pragma once', '#include <Arduino.h>',
            'struct VietnameseComposition { uint16_t base, first, second, composed; };',
            'static const VietnameseComposition VietnameseCompositions[] PROGMEM = {']
    text += ['    {' + ', '.join(f'0x{x:04X}' for x in r) + '},' for r in records]
    text += ['};', '']
    (OUT / 'VietnameseCompositions.h').write_text('\n'.join(text), encoding='utf-8')
    print(f'Canonical normalization: {len(records)} compositions')

if __name__ == '__main__':
    generate('VietnameseSans28', 'DejaVuSans-Bold.ttf', 28)
    generate('VietnameseSans20', 'DejaVuSans.ttf', 20)
    normalization_table()
