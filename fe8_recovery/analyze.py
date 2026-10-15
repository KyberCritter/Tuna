import struct, sys, os

SAVE = r"F:\dumps\FIREEMBLEM2E [BE8E01].sav"

def checksum32(b):
    add = 0
    xr = 0
    n = len(b) & ~1
    for i in range(0, n, 2):
        v = b[i] | (b[i+1] << 8)
        add += v
        xr ^= v
    return ((add & 0xFFFF) + ((xr & 0xFFFF) << 16)) & 0xFFFFFFFF

def checksum16(b):
    add = 0
    xr = 0
    n = len(b) & ~1
    for i in range(0, n, 2):
        v = b[i] | (b[i+1] << 8)
        add += v
        xr ^= v
    return (add + xr) & 0xFFFF

data = open(SAVE, 'rb').read()
print("File:", SAVE)
print("Size: 0x%X (%d)" % (len(data), len(data)))
print()

# ---- General metadata 0x00..0x63 ----
print("=== General metadata (0x00-0x63) ===")
print("magic[0:8]   :", data[0:8])
print("magic word   : 0x%X" % struct.unpack_from('<I', data, 0x08)[0])
print("magic short  : 0x%X" % struct.unpack_from('<H', data, 0x0C)[0])
print("completion bf: 0x%02X" % data[0x0E])
print("flag byte 0F : 0x%02X" % data[0x0F])
print("playthroughs :", data[0x14:0x20].hex())
print("support 2bit :", data[0x20:0x40].hex())
print("char known   :", data[0x40:0x60].hex())
stored16 = struct.unpack_from('<H', data, 0x60)[0]
calc16 = checksum16(data[0x00:0x50])
print("checksum16   : stored=0x%04X calc(first 0x50)=0x%04X match=%s" % (stored16, calc16, stored16 == calc16))
print("last slot    :", data[0x62])
print("save 2nd susp:", data[0x63])
print()

# ---- Save metadata array 0x64, 7 entries ----
print("=== Save metadata array (0x64, 7 x 0x10) ===")
for i in range(7):
    off = 0x64 + i * 0x10
    magic = struct.unpack_from('<I', data, off + 0x00)[0]
    magic2 = struct.unpack_from('<H', data, off + 0x04)[0]
    btype = data[off + 0x06]
    pad = data[off + 0x07]
    blkoff = struct.unpack_from('<H', data, off + 0x08)[0]
    blksize = struct.unpack_from('<H', data, off + 0x0A)[0]
    ck = struct.unpack_from('<I', data, off + 0x0C)[0]
    calc = None
    ok = None
    if blkoff and blksize and blkoff + blksize <= len(data):
        calc = checksum32(data[blkoff:blkoff + blksize])
        ok = (calc == ck)
    print("entry %d @0x%03X: magic=0x%08X magic2=0x%04X type=0x%02X pad=0x%02X off=0x%04X size=0x%04X cksum=0x%08X calc=%s match=%s"
          % (i, off, magic, magic2, btype, pad, blkoff, blksize, ck,
             ("0x%08X" % calc) if calc is not None else "--", ok))
print()

# ---- Known block layout ----
print("=== Full-file block map (from FEUniverse 0x1605) ===")
layout = [
    (0x0000, 0x00D4, "global header / general metadata"),
    (0x00D4, 0x1F78, "suspend slot backup"),
    (0x204C, 0x1F78, "suspend slot"),
    (0x3FC4, 0x0DC8, "save file 1"),
    (0x4D8C, 0x0DC8, "save file 2"),
    (0x5B54, 0x0DC8, "save file 3"),
    (0x691C, 0x0AE4, "block 5 / late data"),
    (0x7400, 0x0C00, "free space"),
]
for off, size, label in layout:
    seg = data[off:off+size]
    nz = sum(1 for x in seg if x != 0)
    ff = sum(1 for x in seg if x == 0xFF)
    print("0x%04X-0x%04X len=0x%04X %-28s nonzero=%5d 0xFF=%5d" % (off, off+size-1, size, label, nz, ff))
print()

# ---- Scan: compute checksums for every plausible aligned block ----
print("=== Candidate checksums for raw regions (size 0xDC8 save, 0x1F78 suspend) ===")
for name, off, size in [("0x3FC4 save1", 0x3FC4, 0xDC8),
                        ("0x4D8C save2", 0x4D8C, 0xDC8),
                        ("0x5B54 save3", 0x5B54, 0xDC8),
                        ("0x00D4 suspB", 0x00D4, 0x1F78),
                        ("0x204C susp", 0x204C, 0x1F78)]:
    seg = data[off:off+size]
    if len(seg) == size:
        print("%-12s cksum32=0x%08X  nonzero=%5d  first32=%s" % (name, checksum32(seg), sum(1 for x in seg if x != 0), seg[:32].hex()))
print()
