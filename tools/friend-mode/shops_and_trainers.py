"""List Black Gate's merchants, trainers and other paid services.

Reads the usecode file with ucread.py and npc.dat for names, and prints each
NPC whose script calls a routine that takes party gold, with the kind of
routine: shop (ends in the shared buy routine 08F8), train (calls the trainee
picker 0920/0921 or the stat helper 0912), spells, heal/resurrect.
Usage: python3 -I shops_and_trainers.py [path/to/npc.dat]
"""
import os, re, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ucread
NPC_DAT = sys.argv[1] if len(sys.argv) > 1 else os.path.expanduser('~/Library/Application Support/Exult/runtime/blackgate/gamedat/npc.dat')
d=open(NPC_DAT,'rb').read()
names=[m.group().decode() for m in re.finditer(rb'[A-Z][A-Za-z\' ]{2,15}(?=\x00)', d)]
def npc_name(n): return names[n if n <= 144 else n - 2]
funcs=ucread.read_functions()
calls={}; intr={}
for fid,f in funcs.items():
    c=set(); i=set()
    for ins in ucread.disasm(f):
        if ins[1]=='UC_CALL': c.add(f.externs[ins[2][0]])
        if ins[1] in ('UC_CALLIS','UC_CALLI'): i.add(ucread.INTR.get(ins[2][0]))
    calls[fid]=c; intr[fid]=i
def closure(fid, seen=None):
    seen = seen if seen is not None else set()
    if fid in seen or fid not in funcs: return seen
    seen.add(fid)
    for c in calls[fid]: closure(c, seen)
    return seen
def kind(fid):
    cl=closure(fid); ii=set().union(*(intr[x] for x in cl))
    if 'remove_party_items' not in ii: return None
    if 0x08F8 in cl or 'add_party_items' in ii and 'input_numeric_value' in ii: return 'shop'
    if 'add_spell' in ii: return 'spells'
    if 'resurrect' in ii: return 'heal/resurrect'
    if 0x0920 in cl or 0x0921 in cl or 0x0912 in cl: return 'train'
    if 'add_party_items' in ii: return 'sell-to-party'
    return 'pay'
rows=[]
for n in range(1,300):
    fid=0x400+n
    if fid not in funcs: continue
    f=funcs[fid]
    kinds={}
    for c in calls[fid]:
        k=kind(c)
        if k and c!=fid: kinds.setdefault(k,[]).append('%04X'%c)
    if not kinds: continue
    text=f.strings.decode('latin-1')
    hours='business hours' in text or 'during the day' in text or 'Come back' in text
    flags=sorted({ins[2][0] for ins in ucread.disasm(f) if ins[1]=='UC_PUSHF'})
    rows.append((n,npc_name(n),kinds,hours,len(flags)))
for n,nm,k,h,nf in rows:
    print('%3d %-12s %s%s' % (n,nm,'; '.join('%s %s'%(a,' '.join(b)) for a,b in sorted(k.items())), '  [hours]' if h else ''))
print(len(rows),'NPCs')
from collections import Counter
print(Counter(a for _,_,k,_,_ in rows for a in k))
