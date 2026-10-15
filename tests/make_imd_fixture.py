"""Generate IMD fixtures: reordered sectors, compressed fills, missing and CRC-bad records."""
from pathlib import Path
import sys

raw = Path(sys.argv[1]).read_bytes()
out = Path(sys.argv[2])
variant = sys.argv[3] if len(sys.argv) > 3 else "normal"
assert len(raw) == 368640

# This is a minimal but spec-correct ImageDisk v1.x file using 250kbps MFM.
data = bytearray(b"IMD 1.18: 15/10/2026 00:00:00\r\nVW recovery regression\r\n\x1a")
for track in range(80):
    # Deliberately scramble physical order; images must be sorted by sector ID.
    ids = list(range(17,-1,-1))
    # Optional cylinder/head maps should not change sector placement.
    head = 0xC0 if track == 1 else 0
    data.extend(bytes((5, track, head, 18, 1)))
    data.extend(ids)
    if head & 0x80:
        data.extend(bytes([track])*18)
    if head & 0x40:
        data.extend(bytes([0])*18)
    for sector in ids:
        blk = raw[(track*18+sector)*256:(track*18+sector+1)*256]
        # On the synthetic test disk, header at 31:1, text at 31:2
        if variant=="bad-data" and track==31 and sector==2:
            data.append(5)    # regular payload but marked CRC bad
            data.extend(blk)
        elif variant=="missing-data" and track==31 and sector==2:
            data.append(0)    # missing sector, no payload
        elif len(set(blk))==1:
            data.extend((2,blk[0]))  # compressed sector
        else:
            data.append(1)
            data.extend(blk)
out.write_bytes(data)
