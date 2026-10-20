#!/usr/bin/env python3
"""Generate a simple 512x512 RGB PNG icon without third-party packages."""
import struct
import sys
import zlib

OUT = sys.argv[1] if len(sys.argv) > 1 else "sce_sys/icon0.png"
W = H = 512
rows = []
for y in range(H):
    row = bytearray([0])
    for x in range(W):
        # Deep navy background with a cyan frame and a blocky T motif.
        edge = x < 12 or y < 12 or x >= W - 12 or y >= H - 12
        bar = (90 <= y <= 155 and 115 <= x <= 397) or (215 <= x <= 280 and 170 <= y <= 395)
        if edge or bar:
            row.extend((42, 220, 220))
        else:
            row.extend((8, 14, 25))
    rows.append(bytes(row))

def chunk(kind, data):
    return struct.pack(">I", len(data)) + kind + data + struct.pack(">I", zlib.crc32(kind + data) & 0xffffffff)

with open(OUT, "wb") as f:
    f.write(b"\x89PNG\r\n\x1a\n")
    f.write(chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0)))
    f.write(chunk(b"IDAT", zlib.compress(b"".join(rows), 9)))
    f.write(chunk(b"IEND", b""))
