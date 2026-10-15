import struct, shutil, os
SRC = r"F:\dumps\FIREEMBLEM2E [BE8E01].sav"
OUT = r"F:\dumps\FIREEMBLEM2E [BE8E01] recovered.sav"
data = open(OUT, 'rb').read()
print("output size:", len(data))

print("\nNon-0xFF bytes in arena/rank/soundroom/bonus region 0x691C..0x73FF:")
for i in range(0x691C, 0x7400):
    if data[i] != 0xFF:
        print("  0x%04X = 0x%02X" % (i, data[i]))

print("\nNon-0xFF bytes in xmap region 0x7400..0x7FFF: %d"
      % sum(1 for i in range(0x7400, 0x8000) if data[i] != 0xFF))

print("\nGame save slots:")
for idx, off in enumerate((0x3FC4, 0x4D8C, 0x5B54)):
    seg = data[off:off+0xDC8]
    print("  slot %d @0x%04X: nonzero=%d  allFF=%s" % (idx+1, off, sum(1 for x in seg if x), all(x == 0xFF for x in seg)))

# copy next to the project for convenience
dst = r"D:\Coding\Tuna\fe8_recovery\FIREEMBLEM2E [BE8E01] recovered.sav"
shutil.copyfile(OUT, dst)
print("\ncopied to:", dst)
print("exists:", os.path.exists(dst))
