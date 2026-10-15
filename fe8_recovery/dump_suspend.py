import struct

SAVE = r"F:\dumps\FIREEMBLEM2E [BE8E01].sav"
data = open(SAVE, 'rb').read()

def hexdump(off, length):
    for i in range(off, off+length, 16):
        chunk = data[i:i+16]
        if not chunk: break
        hexs = ' '.join('%02X' % b for b in chunk)
        asc = ''.join(chr(b) if 32 <= b < 127 else '.' for b in chunk)
        print('%04X  %-47s  %s' % (i, hexs, asc))

print("############ SUSPEND PRIMARY @0x204C ############")
print("First 0x300 bytes:")
hexdump(0x204C, 0x300)
print()
print("############ SUSPEND BACKUP @0x00D4 ############")
print("First 0x100 bytes:")
hexdump(0x00D4, 0x100)
print()
print("############ block5 @0x691C ############")
print("First 0x200 bytes:")
hexdump(0x691C, 0x200)

# Look for ASCII-ish strings in suspend region
import re
for name, off, size in [("suspend@0x204C",0x204C,0x1F78), ("suspendB@0x00D4",0x00D4,0x1F78), ("block5@0x691C",0x691C,0x0AE4)]:
    seg = data[off:off+size]
    strings = re.findall(rb'[ -~]{5,}', seg)
    print("\nASCII runs >=5 in %s: %d" % (name, len(strings)))
    for s in strings[:40]:
        print("   ", s)
