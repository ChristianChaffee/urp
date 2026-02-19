#!/usr/bin/env python3
"""Find 'Packet was modified' in samp03svr and xrefs."""
import subprocess
path = "samp03svr"
s = b"Packet was modified"
with open(path, "rb") as f:
    data = f.read()
off = data.find(s)
if off < 0:
    print("NOT FOUND")
    raise SystemExit(1)
# VA = 0x08048000 + off (first LOAD segment)
va = 0x08048000 + off
print("string at file offset", hex(off), "=> VA", hex(va))
# objdump -d and find lines that reference this VA (e.g. 3b a8 15 08 for 0x0815a83b)
# Find references: raw bytes of VA (little-endian) in file
addr_bytes = bytes([(va >> (i*8)) & 0xFF for i in range(4)])
idx = 0
refs = []
while True:
    i = data.find(addr_bytes, idx)
    if i < 0:
        break
    refs.append(i)
    idx = i + 1
print("References to string (file offsets):", [hex(r) for r in refs])
# First LOAD is 0x08048000, file 0 to 0x137ef8. So code refs are in 0-0x137ef8.
# If ref is in first segment, code VA = 0x08048000 + ref
for r in refs:
    if r < 0x137ef8:
        code_va = 0x08048000 + r
        print("  code VA", hex(code_va))
# mov eax, imm32 = B8 + 4 bytes; push imm32 = 68 + 4 bytes
for op in [0xb8, 0x68]:
    pat = bytes([op]) + addr_bytes
    idx = 0
    while True:
        i = data.find(pat, idx)
        if i < 0:
            break
        print("  op %02x at" % op, hex(i), "=> code VA", hex(0x08048000 + i))
        idx = i + 1
# Try offset 0x11283b as immediate (for base+offset style)
off_bytes = (0x11283b).to_bytes(4, "little")
for op in [0xb8, 0x68]:
    i = data.find(bytes([op]) + off_bytes, 0, 0x137ef8)
    if i >= 0:
        print("  offset ref: op %02x at" % op, hex(i))
