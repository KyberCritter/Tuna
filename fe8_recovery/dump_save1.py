import struct

SAVE = r"F:\dumps\FIREEMBLEM2E [BE8E01].sav"
data = open(SAVE, 'rb').read()

def hexdump(off, length, base=0):
    for i in range(off, off+length, 16):
        chunk = data[i:i+16]
        if not chunk: break
        hexs = ' '.join('%02X' % b for b in chunk)
        asc = ''.join(chr(b) if 32 <= b < 127 else '.' for b in chunk)
        print('%04X  %-47s  %s' % (i, hexs, asc))

def nonzero_runs(off, size, minrun=1):
    runs = []
    i = off
    end = off + size
    while i < end:
        if data[i] != 0:
            j = i
            while j < end and data[j] != 0:
                j += 1
            runs.append((i, j - i))
            i = j
        else:
            i += 1
    return runs

print("############ SAVE FILE 1 block @0x3FC4 (size 0xDC8) ############")
print("nonzero runs (offset, length):")
for o, l in nonzero_runs(0x3FC4, 0xDC8):
    print("  0x%04X len=%d : %s" % (o, l, data[o:o+min(l,32)].hex()))
print()
print("Raw dump of whole block (0x3FC4-0x4D8B):")
hexdump(0x3FC4, 0xDC8)
