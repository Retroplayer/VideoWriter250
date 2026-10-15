"""Synthetic short-document variant based on VW1 observed 70-byte record.
No copyrighted disk bytes beyond a trivial test greeting are included.
"""
from pathlib import Path
import sys

disk=bytearray(80*18*256)
for lba in (0,1,21,22):
    disk[lba//8] |= 1<<(lba%8)
record=bytearray(64)
record[0:6]=bytes([1,1,0,0,1,0])
record[6:26]=b"Document1".ljust(20,b" ")
record[26:28]=bytes([1,3])
disk[256:320]=record
header=21*256
disk[header:header+4]=bytes([70,0,1,4])
body=22*256
disk[body:body+2]=bytes([1,0])
disk[body+27]=0xff
disk[body+28:body+40]=b"Hello World!"
disk[body+40:body+42]=bytes([0x96,1])
disk[body+68:body+70]=bytes([0xff,0x20])
# Deliberately no WV marker; matching real VW1's lack of that identifier.
Path(sys.argv[1]).write_bytes(disk)
