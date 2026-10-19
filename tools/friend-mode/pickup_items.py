"""List the shapes that can be picked up in Black Gate (weight above 0, not a
creature, egg, barge or building), with their name, shape class, weight
(tenths of a stone) and whether the game data makes them a weapon, armour
or ammunition.  Used to sort things into the targeting groups (loot, decor,
furniture) in exult.cc's Friend_thing_group_of; see
docs/friend-mode/targeting-groups.md.
Usage: python3 -I pickup_items.py [path/to/static]
"""
import os, re, struct, sys

STATIC = sys.argv[1] if len(sys.argv) > 1 else os.path.expanduser(
    '~/Library/Application Support/Exult/blackgate/static/')
tfa = open(os.path.join(STATIC, 'tfa.dat'), 'rb').read()
wgtvol = open(os.path.join(STATIC, 'wgtvol.dat'), 'rb').read()
text = open(os.path.join(STATIC, 'text.flx'), 'rb').read()

count = struct.unpack_from('<I', text, 0x54)[0]
names = []
for i in range(count):
    off, length = struct.unpack_from('<II', text, 0x80 + 8 * i)
    names.append(text[off:off + length].split(b'\0')[0].decode('latin-1') if length else '')


def shapes_in(name, entry_size):
    """The shapes listed in weapons.dat, armor.dat or ammo.dat."""
    data = open(os.path.join(STATIC, name), 'rb').read()
    return {struct.unpack_from('<H', data, 1 + i * entry_size)[0] for i in range(data[0])}


weapons = shapes_in('weapons.dat', 21)
armor = shapes_in('armor.dat', 10)
ammo = shapes_in('ammo.dat', 13)
SKIP = {7, 9, 12, 13, 14}    # Eggs, barges, monsters, humans, buildings.


def plain(name):
    """'/gold coin//s' -> 'gold coin'."""
    return re.sub(r'/([^/]*)//?[^/]*', r'\1', name).strip('/').strip()


for shape in range(150, len(tfa) // 3):
    cls = tfa[shape * 3 + 1] & 15
    weight = wgtvol[shape * 2] if shape * 2 < len(wgtvol) else 0
    if weight == 0 or cls in SKIP:
        continue
    kind = 'weapon' if shape in weapons else 'armour' if shape in armor else 'ammo' if shape in ammo else ''
    name = plain(names[shape]) if shape < len(names) else ''
    print('%4d  %-24s class %2d  weight %3d  %s' % (shape, name, cls, weight, kind))
