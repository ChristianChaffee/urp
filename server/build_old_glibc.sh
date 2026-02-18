#!/bin/bash
# Сборка chathider.so в Docker под glibc 2.31 (Ubuntu 20.04)
# Результат совместим с серверами на старом glibc (без доступа к серверу)
set -e
cd "$(dirname "$0")"

echo "Building chathider.so in Docker (Ubuntu 20.04 / glibc 2.31)..."
docker build -f Dockerfile.build -t chathider-build .
docker run --rm -v "$(pwd):/out" chathider-build cp /build/chathider.so /out/chathider.so

echo "Done: chathider.so (compatible with older glibc)"
echo "Copy to server/plugins/ and add to server.cfg: plugins chathider"
