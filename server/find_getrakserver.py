#!/usr/bin/env python3
"""
Проверка бинарника samp03svr (Linux) на наличие паттерна GetRakServer из KeyListener.
Запуск в WSL: python3 find_getrakserver.py /path/to/samp03svr
Если паттерн найден — хук CHATHIDER_HOOK_GETRAK=1 должен работать.
Если не найден — нужен другой паттерн под твою сборку 0.3.7 R2.
"""

import sys
import os

# Тот же паттерн и маска, что в plugin.cpp (KeyListener Linux)
PATTERN = (
    b"\x04\x24\xff\xff\xff\xff\x89\x75\xff\x89\x5d\xff\xe8\xff\xff\xff\xff\x89\x04\x24\x89"
    b"\xc6\xe8\xff\xff\xff\xff\x89\xf0\x8b\x5d\xff\x8b\x75\xff\x89\xec\x5d\xc3"
)
MASK = "xx????xx?xx?x????xxxxxx????xxxx?xx?xxxx"

FUNC_START_OFFSET = 7  # начало функции = найденный байт - 7


def search(data, pattern, mask):
    n = len(mask)
    for i in range(len(data) - n + 1):
        ok = True
        for j, (b, m) in enumerate(zip(pattern, mask)):
            if m == "x" and data[i + j] != b:
                ok = False
                break
        if ok:
            return i
    return -1


def main():
    if len(sys.argv) < 2:
        print("Usage: python3 find_getrakserver.py <samp03svr>")
        print("  Example: python3 find_getrakserver.py ./samp03svr")
        sys.exit(1)
    path = sys.argv[1]
    if not os.path.isfile(path):
        print("Not a file:", path)
        sys.exit(1)
    with open(path, "rb") as f:
        data = f.read()
    pos = search(data, PATTERN, MASK)
    if pos < 0:
        print("Pattern NOT found in", path)
        print("Your 0.3.7 R2 binary may use different code. Try:")
        print("  objdump -d samp03svr > disasm.txt")
        print("Then search for a small function that returns a global (RakServer) pointer.")
        sys.exit(1)
    func_start = pos - FUNC_START_OFFSET
    if func_start < 0:
        print("Pattern found at file offset 0x%x but func_start would be negative; check pattern." % pos)
        sys.exit(1)
    prologue = data[func_start] if func_start < len(data) else None
    print("Pattern found in", path)
    print("  Match at file offset:  0x%x (%u)" % (pos, pos))
    print("  Function start ( -%d ): 0x%x (%u)" % (FUNC_START_OFFSET, func_start, func_start))
    if prologue == 0x55:
        print("  Prologue at start: 0x55 (push ebp) OK")
    else:
        print("  Prologue at start: 0x%02x (expected 0x55 push ebp) — may be wrong function" % (prologue or 0))
    print()
    print("Plugin with CHATHIDER_HOOK_GETRAK=1 should find this at runtime. Try enabling the hook.")


if __name__ == "__main__":
    main()
