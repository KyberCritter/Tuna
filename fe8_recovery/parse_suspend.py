import struct

SAVE = r"F:\dumps\FIREEMBLEM2E [BE8E01].sav"
data = open(SAVE, 'rb').read()

def u8(o): return data[o]
def u16(o): return struct.unpack_from('<H', data, o)[0]
def u32(o): return struct.unpack_from('<I', data, o)[0]

def bits(v, lo, hi):
    return (v >> lo) & ((1 << (hi - lo + 1)) - 1)

def parse_suspend_unit(base):
    # base -> absolute file offset of unit (0x34 bytes)
    b = data[base:base+0x34]
    if len(b) < 0x34: return None
    u = {}
    u['char'] = b[0x00]
    u['class'] = b[0x01]
    u['rescued'] = b[0x03]
    u['state'] = struct.unpack_from('<I', b, 0x04)[0]
    i1 = struct.unpack_from('<H', b, 0x08)[0]
    i2 = struct.unpack_from('<H', b, 0x0A)[0]
    i3 = struct.unpack_from('<H', b, 0x0C)[0]
    u['items'] = [(v & 0xFF, (v >> 8) & 0x3F) for v in (i1, i2, i3)]
    u['maxhp'] = b[0x0E]; u['hp'] = b[0x0F]; u['exp'] = b[0x10]
    u['ranks'] = list(b[0x12:0x1A])
    u['supports'] = list(b[0x1A:0x21])
    w = struct.unpack_from('<I', b, 0x24)[0]
    u['level'] = bits(w, 0, 4); u['x'] = bits(w, 5, 10); u['y'] = bits(w, 11, 16)
    u['pow'] = bits(w, 17, 21); u['skl'] = bits(w, 22, 26); u['spd'] = bits(w, 27, 31)
    w2 = struct.unpack_from('<I', b, 0x28)[0]
    u['def'] = bits(w2, 0, 4); u['res'] = bits(w2, 5, 9); u['lck'] = bits(w2, 10, 14)
    u['con'] = bits(w2, 15, 19)
    w3 = struct.unpack_from('<I', b, 0x2C)[0]
    u['mov'] = bits(w3, 0, 3)
    u['items'].append((bits(w3, 4, 11), bits(w3, 12, 17)))
    u['items'].append((bits(w3, 18, 25), bits(w3, 26, 31)))
    u['raw'] = b
    return u

def dump_suspend(name, blk):
    print("="*70)
    print("SUSPEND BLOCK: %s  (file base 0x%04X)" % (name, blk))
    print("Chapter-data head: %s" % data[blk:blk+0x4C].hex())
    print("Action/RN state  : %s" % data[blk+0x4C:blk+0x4C+6].hex())
    # 51 player units from +0x84
    base = blk + 0x84
    print("\n-- 51 PLAYER UNITS (+0x84) --")
    print(" idx char cls lvl  x  y  HP mxHP exp   pow skl spd def res lck mov  items(idx,uses)")
    for i in range(51):
        u = parse_suspend_unit(base + i*0x34)
        if u is None: break
        items = ', '.join('%02X:%d' % (idx, uses) for idx, uses in u['items'] if idx)
        print("%4d   %02X  %02X  %2d %2d %2d  %2d %2d  %3d    %2d  %2d  %2d  %2d  %2d  %2d  %2d  %s"
              % (i, u['char'], u['class'], u['level'], u['x'], u['y'], u['hp'], u['maxhp'], u['exp'],
                 u['pow'], u['skl'], u['spd'], u['def'], u['res'], u['lck'], u['mov'], items))
    # 60 enemy/other units from +0xB14
    base2 = blk + 0xB14
    print("\n-- 60 ENEMY/OTHER UNITS (+0xB14) --")
    nz = 0
    for i in range(60):
        u = parse_suspend_unit(base2 + i*0x34)
        if u is None: break
        if any(u['raw']):
            nz += 1
            items = ', '.join('%02X:%d' % (idx, uses) for idx, uses in u['items'] if idx)
            print("%4d   %02X  %02X  %2d %2d %2d  %2d %2d  %3d    %2d  %2d  %2d  %2d  %2d  %2d  %2d  %s"
                  % (i, u['char'], u['class'], u['level'], u['x'], u['y'], u['hp'], u['maxhp'], u['exp'],
                     u['pow'], u['skl'], u['spd'], u['def'], u['res'], u['lck'], u['mov'], items))
    print("  (nonzero enemy/other unit slots: %d)" % nz)
    print("\n-- Convoy items (+0x1944, first 0x63 bytes) --")
    conv = data[blk+0x1944:blk+0x1944+0x63]
    print("  " + ' '.join('%02X' % x for x in conv))
    print("\n-- Turn counts/time (+0x1E54, 0xC0) nonzero runs --")
    seg = data[blk+0x1E54:blk+0x1E54+0xC0]
    for o in range(0, len(seg), 4):
        v = struct.unpack_from('<I', seg, o)[0]
        if v:
            print("   entry %2d @+0x%02X = 0x%08X  (chapter=%d turn=%d time=%d)"
                  % (o//4, o, v, v & 0x7F, (v >> 7) & 0x1FF, v >> 16))
    print()

data2 = None
import sys
if len(sys.argv) > 1:
    dump_suspend("primary", 0x204C)
else:
    dump_suspend("primary", 0x204C)
    dump_suspend("backup", 0x00D4)
