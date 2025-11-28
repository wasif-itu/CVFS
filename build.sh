#!/usr/bin/env bash
set -e
mkdir -p build
gcc -Wall $(pkg-config --cflags fuse3) \
    src/fuse/vfs_fuse.c src/vfs/vfs_core_stub.c -o build/cvfs $(pkg-config --libs fuse3)
echo "Built -> build/cvfs"
