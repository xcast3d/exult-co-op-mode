"""Minimal reader for Ultima VII: The Black Gate's usecode file."""
import os, re, struct, sys

REPO = os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'ucxt', 'data', '')
USECODE = os.environ.get('U7_USECODE', os.path.expanduser('~/Library/Application Support/Exult/blackgate/static/usecode'))

def load_tables():
    t = open(REPO + 'u7opcodes.data').read()
    ops = {}
    for m in re.finditer(r'<(0x[0-9A-Fa-f]+)>\s*<name>\s*(\S+)\s*</>.*?<num_bytes>\s*(\d+)\s*</>\s*<param_types>\s*\{([^}]*)\}', t, re.S):
        code = int(m.group(1), 16)
        if code not in ops:
            ops[code] = (m.group(2), int(m.group(3)), [p.strip() for p in m.group(4).split(',') if p.strip()])
    intr = {}
    for m in re.finditer(r'<(0x[0-9A-Fa-f]+)>\s*(\S+)\s*</>', open(REPO + 'u7bgintrinsics.data').read()):
        intr[int(m.group(1), 16)] = m.group(2)
    return ops, intr

OPS, INTR = load_tables()

class Func:
    pass

def read_functions(path=USECODE):
    data = open(path, 'rb').read()
    pos, funcs = 0, {}
    while pos + 4 <= len(data):
        fid, size = struct.unpack_from('<HH', data, pos)
        body = data[pos + 4:pos + 4 + size]
        pos += 4 + size
        dsize = struct.unpack_from('<H', body, 0)[0]
        strings = body[2:2 + dsize]
        p = 2 + dsize
        nargs, nlocals, nexterns = struct.unpack_from('<HHH', body, p)
        p += 6
        externs = list(struct.unpack_from('<%dH' % nexterns, body, p))
        p += 2 * nexterns
        f = Func()
        f.id, f.strings, f.externs, f.code = fid, strings, externs, body[p:]
        f.nargs, f.nlocals = nargs, nlocals
        funcs[fid] = f
    return funcs

def string_at(f, off):
    end = f.strings.find(b'\0', off)
    return f.strings[off:end].decode('latin-1')

def disasm(f):
    """Yield (offset, name, operands) for each instruction."""
    code, i = f.code, 0
    while i < len(code):
        op = code[i]
        if op not in OPS:
            yield (i, 'UNKNOWN_%02X' % op, [])
            i += 1
            continue
        name, nbytes, types = OPS[op]
        raw = code[i + 1:i + 1 + nbytes]
        vals, j = [], 0
        for t in types:
            if t == 'byte':
                vals.append(raw[j]); j += 1
            elif t in ('long', 'offset32', 'dataoffset32'):
                vals.append(struct.unpack_from('<i', raw, j)[0]); j += 4
            else:
                v = struct.unpack_from('<H', raw, j)[0]
                if t == 'offset':
                    v = struct.unpack_from('<h', raw, j)[0]
                vals.append(v); j += 2
        nxt = i + 1 + nbytes
        # Relative offsets are from the next instruction.
        vals = [nxt + v if t == 'offset' else v for v, t in zip(vals, types)]
        yield (i, name, vals)
        i = nxt

def describe(f, ins):
    off, name, vals = ins
    if name in ('UC_PUSHS', 'UC_ADDSI'):
        return '%s %r' % (name, string_at(f, vals[0]))
    if name == 'UC_CALL':
        return 'UC_CALL fn %04X' % f.externs[vals[0]]
    if name in ('UC_CALLIS', 'UC_CALLI'):
        return '%s %s(%d)' % (name, INTR.get(vals[0], hex(vals[0])), vals[1])
    if name in ('UC_PUSHF', 'UC_POPF'):
        return '%s flag %d' % (name, vals[0])
    return '%s %s' % (name, ' '.join(str(v) for v in vals))

if __name__ == '__main__':
    funcs = read_functions()
    fid = int(sys.argv[1], 16)
    f = funcs[fid]
    for ins in disasm(f):
        print('%5d  %s' % (ins[0], describe(f, ins)))
