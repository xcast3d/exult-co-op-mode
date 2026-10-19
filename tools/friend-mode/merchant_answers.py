"""List the conversation answers a Friend Mode guest may see with each
merchant and trainer: name, job, bye, and the answers that lead to buying,
selling to the party or training (and the choices those add, such as a
shop's "weapons" and "armour").  Story answers ("murder", "Fellowship")
are left out.

Reads the usecode file with ucread.py.  Each NPC's script is a list of
cases, `PUSHS "word"; CMPS 1 <end>`; a word is a trade word if its case
calls a shop, trainer or sell-to-party routine, or adds an answer that is
one.  Prints the C++ table for exult.cc's friend_merchant_answers.
Usage: python3 -I merchant_answers.py
"""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ucread

# Must match friend_merchants in exult.cc.
MERCHANTS = [18, 19, 24, 37, 38, 47, 50, 51, 52, 53, 54, 55, 59, 67, 74, 79, 87, 92, 102, 118, 121, 123,
             130, 153, 163, 179, 181, 188, 189, 190, 194, 196, 198, 199, 206, 207, 215, 216, 219, 222, 229, 231, 237,
             7, 20, 36, 49, 76, 94, 95, 104, 115, 119, 159, 161, 192, 205, 228, 238]
ALWAYS = ['name', 'job', 'bye']
TRADE = {'shop', 'train', 'sell-to-party'}

funcs = ucread.read_functions()
calls, intr = {}, {}
for fid, f in funcs.items():
    c, i = set(), set()
    for ins in ucread.disasm(f):
        if ins[1] == 'UC_CALL':
            c.add(f.externs[ins[2][0]])
        if ins[1] in ('UC_CALLIS', 'UC_CALLI'):
            i.add(ucread.INTR.get(ins[2][0]))
    calls[fid], intr[fid] = c, i

def closure(fid, seen=None):
    seen = seen if seen is not None else set()
    if fid in seen or fid not in funcs:
        return seen
    seen.add(fid)
    for c in calls[fid]:
        closure(c, seen)
    return seen

def kind(fid):
    """As shops_and_trainers.py."""
    cl = closure(fid)
    ii = set().union(*(intr[x] for x in cl))
    if 'remove_party_items' not in ii:
        return None
    if 0x08F8 in cl or 'add_party_items' in ii and 'input_numeric_value' in ii:
        return 'shop'
    if 'add_spell' in ii:
        return 'spells'
    if 'resurrect' in ii:
        return 'heal/resurrect'
    if 0x0920 in cl or 0x0921 in cl or 0x0912 in cl:
        return 'train'
    if 'add_party_items' in ii:
        return 'sell-to-party'
    return 'pay'

def answers_for(npc):
    f = funcs[0x400 + npc]
    code = list(ucread.disasm(f))
    cases = []    # (word, start, end)
    for k in range(1, len(code)):
        off, name, vals = code[k]
        if name == 'UC_CMPS' and code[k - 1][1] == 'UC_PUSHS':
            word = ucread.string_at(f, code[k - 1][2][0])
            start = code[k + 1][0] if k + 1 < len(code) else off
            cases.append((word, start, vals[-1]))
    info = {}
    for word, start, end in cases:
        added, run, trade, leaves = set(), [], False, False
        for off, name, vals in code:
            if not start <= off < end:
                continue
            if name == 'UC_PUSHS':
                run.append(ucread.string_at(f, vals[0]))
                continue
            if name == 'UC_CALLI' and ucread.INTR.get(vals[0]) == 'add_answer':
                added.update(run)
            if name == 'UC_CALL' and kind(f.externs[vals[0]]) in TRADE:
                trade = True
            if name == 'UC_CALLI' and ucread.INTR.get(vals[0]) == 'pop_answers':
                leaves = True
            if name not in ('UC_ARRC',):
                run = []
        old = info.get(word, (False, set(), False))
        info[word] = (old[0] or trade, old[1] | added, old[2] or leaves)
    trade_words = {w for w, (t, _, _) in info.items() if t}
    # A word that adds a trade word leads to one ("buy" adds "weapons").
    # Name, job and bye always show, and lead nowhere by themselves.
    changed = True
    while changed:
        changed = False
        for w, (_, added, _) in info.items():
            if w not in trade_words and w not in ALWAYS and added & trade_words:
                trade_words.add(w)
                changed = True
    # Plus the way out of a shop's list ("nothing", which drops the list).
    exits = {w for w, (_, _, leaves) in info.items() if leaves and w not in trade_words}
    allowed = set(ALWAYS) | trade_words
    for w in trade_words:
        allowed |= info[w][1] & exits
    return sorted(allowed), sorted(trade_words)

missing = []
print('static const std::unordered_map<int, std::vector<std::string>> friend_merchant_answers{')
for npc in sorted(MERCHANTS):
    allowed, trade = answers_for(npc)
    if not trade:
        missing.append(npc)
    print('\t\t{%d, {%s}},' % (npc, ', '.join('"%s"' % w.replace('"', '\\"') for w in allowed)))
print('};')
if missing:
    print('// No trade word found for:', missing, file=sys.stderr)
