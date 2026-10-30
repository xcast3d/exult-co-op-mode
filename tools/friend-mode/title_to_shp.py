"""Turn a PNG into the title menu's Quest of the Companions picture,
data/friend/title.shp (game.cc draws it under Black Gate's logo).

The menu uses Black Gate's intro palette (intropal.dat, palette 6), not
the game's, so each colour is matched to the nearest of its first 224
entries (the rest cycle); pixels under half opaque are transparent (255).
Plain Python, no Pillow: reads 8-bit indexed or RGBA PNGs, then packs the
shape with Exult's own encoder, tools/ipack (build it first: make).

Usage: python3 -I title_to_shp.py picture.png [path/to/static]
With QOTC_TITLE_PREVIEW=out.png set, also writes the matched picture there.
"""
import os
import struct
import subprocess
import sys
import tempfile
import zlib
from pathlib import Path

REPO = Path(__file__).resolve().parents[2]
OUT = REPO / 'data/friend/title.shp'
source = Path(sys.argv[1])
static = Path(sys.argv[2] if len(sys.argv) > 2 else os.path.expanduser(
    '~/Library/Application Support/Exult/blackgate/static/'))


def chunks(data):
    at = 8
    while at < len(data):
        length, kind = struct.unpack_from('>I4s', data, at)
        yield kind, data[at + 8:at + 8 + length]
        at += 12 + length


def read_png(path):
    """Width, height and rows of (r, g, b, a)."""
    data = path.read_bytes()
    assert data[:8] == b'\x89PNG\r\n\x1a\n', 'not a PNG'
    plte, trns, idat = b'', b'', b''
    for kind, body in chunks(data):
        if kind == b'IHDR':
            w, h, depth, colour, _, _, interlace = struct.unpack('>IIBBBBB', body)
        elif kind == b'PLTE':
            plte = body
        elif kind == b'tRNS':
            trns = body
        elif kind == b'IDAT':
            idat += body
    assert depth == 8 and colour in (3, 6) and not interlace, 'needs an 8-bit indexed or RGBA PNG'
    bpp = 1 if colour == 3 else 4
    raw = zlib.decompress(idat)
    stride = w * bpp
    rows, prev, at = [], bytearray(stride), 0
    for _ in range(h):
        kind, line = raw[at], bytearray(raw[at + 1:at + 1 + stride])
        at += 1 + stride
        for i in range(stride):
            left = line[i - bpp] if i >= bpp else 0
            up = prev[i]
            corner = prev[i - bpp] if i >= bpp else 0
            if kind == 1:
                line[i] = (line[i] + left) & 255
            elif kind == 2:
                line[i] = (line[i] + up) & 255
            elif kind == 3:
                line[i] = (line[i] + (left + up) // 2) & 255
            elif kind == 4:
                p = left + up - corner
                pa, pb, pc = abs(p - left), abs(p - up), abs(p - corner)
                line[i] = (line[i] + (left if pa <= pb and pa <= pc else up if pb <= pc else corner)) & 255
        prev = line
        if colour == 3:
            rows.append([tuple(plte[3 * c:3 * c + 3]) + (trns[c] if c < len(trns) else 255,) for c in line])
        else:
            rows.append([tuple(line[i:i + 4]) for i in range(0, stride, 4)])
    return w, h, rows


def intro_palette():
    """Palette 6 of intropal.dat (a FLEX of double palettes: even bytes are
    the colours, 0-63), as 0-255."""
    data = (static / 'intropal.dat').read_bytes()
    offset, length = struct.unpack_from('<II', data, 0x80 + 8 * 6)
    pal = data[offset:offset + length:2][:768]
    return [tuple(min(255, v * 4) for v in pal[3 * i:3 * i + 3]) for i in range(256)]


def write_indexed(path, w, h, pixels, palette):
    def chunk(kind, body):
        return struct.pack('>I', len(body)) + kind + body + struct.pack('>I', zlib.crc32(kind + body))
    raw = b''.join(b'\0' + bytes(pixels[y * w:(y + 1) * w]) for y in range(h))
    path.write_bytes(b'\x89PNG\r\n\x1a\n' + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 3, 0, 0, 0))
                     + chunk(b'PLTE', bytes(v for c in palette for v in c))
                     + chunk(b'tRNS', bytes([255] * 255 + [0]))
                     + chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b''))


w, h, rows = read_png(source)
palette = intro_palette()
cache, pixels = {}, []
for row in rows:
    for r, g, b, a in row:
        if a < 128:
            pixels.append(255)
            continue
        if (r, g, b) not in cache:
            cache[r, g, b] = min(range(224), key=lambda i: sum((c - p) ** 2 for c, p in zip((r, g, b), palette[i])))
        pixels.append(cache[r, g, b])

if os.environ.get('QOTC_TITLE_PREVIEW'):
    write_indexed(Path(os.environ['QOTC_TITLE_PREVIEW']), w, h, pixels, palette)
ipack = REPO / 'tools/ipack'
assert ipack.is_file(), 'build tools/ipack first (make)'
with tempfile.TemporaryDirectory(prefix='qotc-title-') as tmp:
    tmp = Path(tmp)
    write_indexed(tmp / 'title00.png', w, h, pixels, palette)
    script = tmp / 'pack.txt'
    script.write_text(f'archive {tmp}/title.flx\npalette {tmp}/palette.flx\n0/1: {tmp}/title\n')
    subprocess.run([str(ipack), '-c', str(script)], check=True, capture_output=True)
    archive = (tmp / 'title.flx').read_bytes()
    offset, size = struct.unpack_from('<II', archive, 128)
    shape = archive[offset:offset + size]
    assert len(shape) == size and struct.unpack_from('<I', shape)[0] == size
OUT.parent.mkdir(parents=True, exist_ok=True)
OUT.write_bytes(shape)
print(f'{OUT.relative_to(REPO)}: {w}x{h}, {len(cache)} colours matched to the intro palette')
