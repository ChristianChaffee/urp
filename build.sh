#!/bin/bash
# chathider.asi — сборка для SA-MP 0.3.7 R3
# Требуется: MSYS2 MinGW32 (i686-w64-mingw32-g++)

set -e
cd "$(dirname "$0")"

echo "Building chathider.asi..."

i686-w64-mingw32-g++ -shared -o chathider.asi -std=c++11 -O2 -s -fpermissive \
  -I./SAMP-API/include -I./client -I./server -DWIN32 -D_WINDOWS \
  client/main.cpp \
  client/packet_hook.cpp \
  SAMP-API/src/sampapi/sampapi.cpp \
  SAMP-API/src/sampapi/0.3.7-R3-1/CChat.cpp \
  SAMP-API/src/sampapi/0.3.7-R3-1/CNetGame.cpp \
  server/lib/raknet/BitStream.cpp \
  -static -lpsapi

echo "Done: chathider.asi"
echo "Copy to SA-MP folder (next to gta_sa.exe)"
