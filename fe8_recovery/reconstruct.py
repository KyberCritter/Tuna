import struct, os

SRC = r"F:\dumps\FIREEMBLEM2E [BE8E01].sav"
OUT_F = r"F:\dumps\FIREEMBLEM2E [BE8E01] recovered.sav"
OUT_W = r"D:\Coding\Tuna\fe8_recovery\FIREEMBLEM2E [BE8E01] recovered.sav"

data = bytearray(open(SRC, 'rb').read())

SUSP = 0x00D4      # SAVE_ID_SUSPEND
SUSP_ALT = 0x204C  # SAVE_ID_SUSPEND_ALT  (active: global.last_suspend_slot == 1)
GAME0 = 0x3FC4     # SAVE_ID_GAME0
GAMESIZE = 0xDC8
SUSPSIZE = 0x1F78

def checksum32(b):
    add = 0; xr = 0
    n = len(b) & ~1
    for i in range(0, n, 2):
        v = b[i] | (b[i+1] << 8); add += v; xr ^= v
    return ((add & 0xFFFF) + ((xr & 0xFFFF) << 16)) & 0xFFFFFFFF

def checksum16(b):
    add = 0; xr = 0
    n = len(b) & ~1
    for i in range(0, n, 2):
        v = b[i] | (b[i+1] << 8); add += v; xr ^= v
    return (add + xr) & 0xFFFF

def s8(x):
    return x - 256 if x > 127 else x

# ---------- parse suspend unit ----------
def parse_sus_unit(blk, i):
    o = blk + 0x84 + i * 0x34
    b = data[o:o+0x34]
    u = {}
    u['pid'] = b[0x00]; u['jid'] = b[0x01]; u['ai1'] = b[0x02]; u['rescue'] = b[0x03]
    u['state'] = struct.unpack_from('<I', b, 0x04)[0]
    i1, i2, i3 = struct.unpack_from('<HHH', b, 0x08)
    u['maxHP'] = s8(b[0x0E]); u['curHP'] = s8(b[0x0F]); u['exp'] = b[0x10]
    u['ranks'] = list(b[0x12:0x1A])
    u['supports'] = list(b[0x1A:0x21])
    w = struct.unpack_from('<I', b, 0x24)[0]
    u['level'] = w & 0x1F; u['x'] = (w >> 5) & 0x3F; u['y'] = (w >> 11) & 0x3F
    u['pow'] = (w >> 17) & 0x1F; u['skl'] = (w >> 22) & 0x1F; u['spd'] = (w >> 27) & 0x1F
    w = struct.unpack_from('<I', b, 0x28)[0]
    u['def'] = w & 0x1F; u['res'] = (w >> 5) & 0x1F; u['lck'] = (w >> 10) & 0x1F
    u['con'] = (w >> 15) & 0x1F
    w = struct.unpack_from('<I', b, 0x2C)[0]
    u['mov'] = w & 0xF
    u['items'] = [i1 & 0x3FFF, i2 & 0x3FFF, i3 & 0x3FFF, (w >> 4) & 0x3FFF, (w >> 18) & 0x3FFF]
    return u

# ---------- pack game unit ----------
GFIELDS = [('jid',7),('level',5),('exp',7),('xPos',6),('yPos',6),('flag',13),
           ('max_hp',6),('pow',5),('skl',5),('spd',5),('def',5),('res',5),
           ('lck',5),('con_bonus',5),('mov_bonus',5),
           ('item1',14),('item2',14),('item3',14),('item4',14),('item5',14)]

def pack_game_unit(v):
    out = bytearray(0x24)
    bit = 0
    for name, width in GFIELDS:
        val = v.get(name, 0) & ((1 << width) - 1)
        for k in range(width):
            if (val >> k) & 1:
                out[bit >> 3] |= 1 << (bit & 7)
            bit += 1
        assert bit <= 160
    out[0x14] = v['pid'] & 0xFF
    for i, r in enumerate(v.get('wpnRanks', [0]*8)):
        out[0x15 + i] = r & 0xFF
    for i, s in enumerate(v.get('supports', [0]*7)):
        out[0x1D + i] = s & 0xFF
    return out

US_DEAD = 1 << 2; US_ND = 1 << 3; US_S1 = 1 << 14; US_S2 = 1 << 15
US_MET = 1 << 13; US_B16 = 1 << 16; US_B25 = 1 << 25; US_B21 = 1 << 21
def state_to_flag(st):
    f = 0
    if st & US_DEAD: f |= 1 << 0
    if st & US_ND:   f |= 1 << 1
    if st & US_S1:   f |= 1 << 2
    if st & US_S2:   f |= 1 << 3
    if st & US_MET:  f |= 1 << 4
    if st & US_B16:  f |= 1 << 5
    if st & US_B25:  f |= 1 << 6
    if st & US_B21:  f |= 1 << 7
    return f

# ---------- what is in the suspend? ----------
print("=== ACTIVE SUSPEND (0x%04X) CONTENTS ===" % SUSP_ALT)
ps = data[SUSP_ALT:SUSP_ALT+0x4C]
CHAP = {0:'Prologue: The Fall of Renais',1:'Ch1: Escape!',2:'Ch2: The Protected'}
print("time_saved        = %d" % struct.unpack_from('<I', ps, 0x00)[0])
print("time_chapter_start= %d" % struct.unpack_from('<I', ps, 0x04)[0])
print("partyGold         = %d" % struct.unpack_from('<I', ps, 0x08)[0])
print("gameSaveSlot      = %d" % ps[0x0C])
print("chapterIndex      = %d (%s)" % (ps[0x0E], CHAP.get(ps[0x0E], '?')))
print("chapterTurnNumber = %d" % struct.unpack_from('<H', ps, 0x10)[0])
print("xCursor,yCursor   = %d,%d" % (ps[0x12], ps[0x13]))
print("chapterStateBits  = 0x%02X" % ps[0x14])
print("chapterModeIndex  = %d" % ps[0x1B])
print()

# ---------- build GameSaveBlock ----------
g = bytearray(data[GAME0:GAME0+GAMESIZE])      # start from the valid blank slot-1 block

g[0x00:0x4C] = ps                              # PlaySt <- suspend
g[0x0C] = 0                                    # gameSaveSlot = 0

roster = []
for i in range(51):
    u = parse_sus_unit(SUSP_ALT, i)
    dst = 0x4C + i * 0x24
    if u['pid'] == 0:
        g[dst:dst+0x24] = b'\x00' * 0x24
        continue
    roster.append((i, u))
    vals = {
        'jid': u['jid'], 'level': u['level'], 'exp': u['exp'],
        'xPos': u['x'], 'yPos': u['y'], 'flag': state_to_flag(u['state']),
        'max_hp': max(0, u['maxHP']), 'pow': u['pow'], 'skl': u['skl'],
        'spd': u['spd'], 'def': u['def'], 'res': u['res'], 'lck': u['lck'],
        'con_bonus': u['con'], 'mov_bonus': u['mov'],
        'item1': u['items'][0], 'item2': u['items'][1], 'item3': u['items'][2],
        'item4': u['items'][3], 'item5': u['items'][4],
        'pid': u['pid'], 'wpnRanks': u['ranks'], 'supports': u['supports'],
    }
    g[dst:dst+0x24] = pack_game_unit(vals)

# supply items / BWL stats / chapter stats / permanent flags
g[0x79C:0x84C] = data[SUSP_ALT+0x1944:SUSP_ALT+0x1944+0xB0]
g[0x84C:0xCAC] = data[SUSP_ALT+0x19F4:SUSP_ALT+0x19F4+0x460]
g[0xCAC:0xD6C] = data[SUSP_ALT+0x1E54:SUSP_ALT+0x1E54+0xC0]
g[0xD6C:0xD85] = data[SUSP_ALT+0x1F24:SUSP_ALT+0x1F24+0x19]

print("=== RECOVERED ROSTER (written to save slot 1) ===")
NAME = {1:'Eirika', 2:'Seth'}
CLS  = {1:'Ephraim Lord', 2:'Eirika Lord', 7:'Paladin'}
for i, u in roster:
    print(" slot %2d: pid=%d %-7s jid=%d %-12s Lv%d HP%d  x=%d y=%d  items=%s  supports=%s"
          % (i, u['pid'], NAME.get(u['pid'], '?'), u['jid'], CLS.get(u['jid'], '?'),
             u['level'], u['maxHP'], u['x'], u['y'],
             ['%02X' % it for it in u['items'] if it], u['supports']))
print()

# ---------- commit ----------
data[GAME0:GAME0+GAMESIZE] = g

# SaveBlockInfo entry 0
struct.pack_into('<I', data, 0x64 + 0x00, 0x40624)   # magic32
struct.pack_into('<H', data, 0x64 + 0x04, 0x200A)    # magic16
data[0x64 + 0x06] = 0x00                              # kind = GAME
data[0x64 + 0x07] = 0x00
struct.pack_into('<H', data, 0x64 + 0x08, 0x3FC4)     # offset
struct.pack_into('<H', data, 0x64 + 0x0A, 0xDC8)      # size
struct.pack_into('<I', data, 0x64 + 0x0C, checksum32(g))

# GlobalSaveInfo: mark Eirika/Seth known, refresh 16-bit checksum
for pid in (1, 2):
    data[0x40 + (pid >> 3)] |= 1 << (pid & 7)
struct.pack_into('<H', data, 0x60, checksum16(data[0x00:0x50]))
data[0x62] = 0     # last_game_save_id
# keep 0x63 (last_suspend_slot)=1 so the suspend stays resumable

# ---------- verify ----------
ok = checksum32(data[GAME0:GAME0+GAMESIZE]) == struct.unpack_from('<I', data, 0x64+0x0C)[0]
print("save-block checksum valid :", ok)
print("global checksum valid     :", checksum16(data[0:0x50]) == struct.unpack_from('<H', data, 0x60)[0])
for i in range(5):
    off = 0x64 + i*0x10
    m = struct.unpack_from('<I', data, off)[0]
    k = data[off+6]
    if m == 0x40624:
        o = struct.unpack_from('<H', data, off+8)[0]
        s = struct.unpack_from('<H', data, off+0x0A)[0]
        c = struct.unpack_from('<I', data, off+0x0C)[0]
        print("  meta[%d]: kind=%d off=0x%04X size=0x%04X checksum %s"
              % (i, k, o, s, 'OK' if checksum32(data[o:o+s]) == c else 'BAD'))

out = OUT_F
try:
    open(out, 'wb').write(data)
except Exception as e:
    print("F: write failed (%s); writing to project dir" % e)
    out = OUT_W
    open(out, 'wb').write(data)
print("wrote:", out)
