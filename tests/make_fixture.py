"""Create a deliberately tiny, synthetic VideoWriter-shaped disk for CI.

Not a real machine-produced image; only exercises the current extractor.
"""
import sys
from pathlib import Path

SECTOR = 256
SPT = 18
image = bytearray(80 * SPT * SECTOR)
image[0xBE:0xC0] = b"5F"  # genuine document disks need not contain WV
header = 31 * SPT + 1
data = header + 1
for sector in (0, 1, header, data):
    image[sector // 8] |= 1 << (sector % 8)

directory = SECTOR
image[directory] = 1
name = b"Test Document"
image[directory + 6:directory + 26] = name.ljust(20, b" ")
image[directory + 26] = 31
image[directory + 27] = 1

message = b"Grandma test note"
header_pos = header * SECTOR
image[header_pos:header_pos + 4] = bytes([len(message), 0, 31, 2])
data_pos = data * SECTOR
image[data_pos:data_pos + len(message)] = message
Path(sys.argv[1]).write_bytes(image)
