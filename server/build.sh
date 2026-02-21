#!/bin/bash
# chathider.so — SA-MP 0.3.7 R2 Linux server plugin
# Сборка на Linux (32-bit, как samp03svr)

set -e
cd "$(dirname "$0")"

echo "Building chathider.so..."

# -static-libstdc++: не требовать GLIBCXX_3.4.29 на сервере (совместимость со старым libstdc++)
g++ -shared -o chathider.so -m32 -O2 -fPIC -std=c++17 -static-libstdc++ \
    -DLINUX \
    -I. \
    plugin.cpp \
    lib/raknet/BitStream.cpp

echo "Done: chathider.so"
echo "Copy to server/plugins/ and add to server.cfg: plugins chathider"
