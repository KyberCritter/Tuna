import struct
OUT = r"F:\dumps\FIREEMBLEM2E [BE8E01] recovered.sav"
data = open(OUT, 'rb').read()
GAME0 = 0x3FC4

GFIELDS = [('jid',7),('level',5),('exp',7),('xPos',6),('yPos',6),('flag',13),
           ('max_hp',6),('pow',5),('skl',5),('spd',5),('def',5),('res',5),
           ('lck',5),('con_bonus',5),('mov_bonus',5),
           ('item1',14),('item2',14),('item3',14),('item4',14),('item5',14)]

def unpack_game_unit(o):
    b = data[o:o+0x24]
    out = {}
    bit = 0
    for name, width in GFIELDS:
        v = 0
        for k in range(width):
            if b[bit >> 3] & (1 << (bit & 7)):
                v |= 1 << k
            bit += 1
        out[name] = v
    out['pid'] = b[0x14]
    out['ranks'] = list(b[0x15:0x1D])
    out['supports'] = list(b[0x1D:0x24])
    return out

print("=== DECODED slot-1 units (game GameSavePackedUnit layout) ===")
for i in range(51):
    u = unpack_game_unit(GAME0 + 0x4C + i*0x24)
    if u['pid'] == 0:
        continue
    print("slot %2d pid=%d jid=%d Lv%d exp=%d x=%d y=%d HP=%d pow=%d skl=%d spd=%d def=%d res=%d lck=%d con=%d mov=%d flag=0x%X"
          % (i, u['pid'], u['jid'], u['level'], u['exp'], u['xPos'], u['yPos'], u['max_hp'],
             u['pow'], u['skl'], u['spd'], u['def'], u['res'], u['lck'], u['con_bonus'],
             u['mov_bonus'], u['flag']))
    print("        items:", ['%04X(idx=%02X,uses=%d)' % (u[k], u[k] & 0xFF, (u[k] >> 8) & 0x3F)
                             for k in ('item1','item2','item3','item4','item5')])
    print("        ranks:", ['%02X' % r for r in u['ranks']], " supports:", u['supports'])

# playSt decode of the new save
ps = data[GAME0:GAME0+0x4C]
print("\n=== new slot-1 PlaySt ===")
print(" time_saved=%d time_chapter_started=%d gold=%d slot=%d vision=%d chapter=%d faction=%d turn=%d cursor=%d,%d state=0x%02X weather=%d pid=%d mode=%d"
      % (struct.unpack_from('<I', ps,0)[0], struct.unpack_from('<I', ps,4)[0],
         struct.unpack_from('<I', ps,8)[0], ps[0x0C], ps[0x0D], ps[0x0E], ps[0x0F],
         struct.unpack_from('<H', ps,0x10)[0], ps[0x12], ps[0x13], ps[0x14], ps[0x15],
         ps[0x18], ps[0x1B]))
