import struct

SAVE = r"F:\dumps\FIREEMBLEM2E [BE8E01].sav"
data = open(SAVE, 'rb').read()

A = 0x00D4   # suspend backup (SAVE_ID_SUSPEND)
B = 0x204C   # suspend alt    (SAVE_ID_SUSPEND_ALT)
L = 0x1F78

print("### DIFF of the two suspend blocks ###")
diffs = [i for i in range(L) if data[A+i] != data[B+i]]
print("differing bytes: %d / %d (%.2f%%)" % (len(diffs), L, 100.0*len(diffs)/L))
# group into runs
runs = []
if diffs:
    s = diffs[0]; p = diffs[0]
    for i in diffs[1:]:
        if i == p+1: p = i
        else:
            runs.append((s,p)); s=i; p=i
    runs.append((s,p))
print("differing runs (%d):" % len(runs))
for s,p in runs[:60]:
    print("   +0x%04X..+0x%04X (len %d):  A=%s  B=%s" % (s,p,p-s+1,
          data[A+s:A+p+1].hex(), data[B+s:B+p+1].hex()))
print()

def validunit(o):
    pid = data[o+0]; jid = data[o+1]
    return (1 <= pid <= 0x40) and (0 <= jid <= 0x60)

print("### Scan: which start offset makes the 51-unit array look sane? ###")
for base_off in range(0x40, 0xC0, 2):
    cnt = 0
    for blk, nm in ((A,'A'),(B,'B')):
        o0 = blk + base_off
        c = 0
        for i in range(51):
            o = o0 + i*0x34
            if o+0x34 <= blk+L and validunit(o):
                c += 1
        if nm == 'A': cntA = c
        else: cntB = c
    if cntA >= 2 or cntB >= 2:
        print("  base +0x%03X : valid units A=%2d/51  B=%2d/51" % (base_off, cntA, cntB))
print()

print("### Raw 0x34-byte records starting at +0x84 (B, primary) ###")
for i in range(6):
    o = B + 0x84 + i*0x34
    rec = data[o:o+0x34]
    print("unit %d @0x%04X: %s" % (i, o, rec.hex()))
