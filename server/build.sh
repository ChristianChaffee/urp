#!/bin/bash
# chathider.so — SA-MP 0.3.7 R2 Linux server plugin
# Сборка на Linux (32-bit, как samp03svr)

set -e
cd "$(dirname "$0")"

echo "Building chathider.so..."

g++ -shared -o chathider.so -m32 -O2 -fPIC -std=c++11 \
    -DLINUX \
    -I. \
    plugin.cpp \
    lib/raknet/BitStream.cpp

echo "Done: chathider.so"
echo "Copy to server/plugins/ and add to server.cfg: plugins chathider"
