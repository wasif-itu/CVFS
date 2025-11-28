#!/usr/bin/env bash
set -euo pipefail

# CVFS unified build/run/test helper
# Usage:
#   ./build.sh build           # clean build via Makefile
#   ./build.sh test            # run unit + integration tests
#   ./build.sh valgrind        # run valgrind leak checks
#   ./build.sh fuse:start      # mount FUSE FS at /tmp/vfs_mount
#   ./build.sh fuse:stop       # unmount FUSE FS
#   ./build.sh fuse:status     # show mount status
#   ./build.sh fuse:shell      # open a subshell rooted at /tmp/vfs_mount
#   ./build.sh demo            # quick build + mount + list + unmount

MOUNT_DIR=/tmp/vfs_mount

ensure_deps() {
    command -v pkg-config >/dev/null || { echo "pkg-config missing"; exit 1; }
    pkg-config --exists fuse3 || { echo "fuse3 dev libs missing"; exit 1; }
}

cmd_build() {
    ensure_deps
    make clean
    make
}

cmd_test() {
    make test_core
    make test_lookup
    make test_file_ops
    make test_integration
    make test_stress
}

cmd_valgrind() {
    ./tests/run_valgrind.sh
}

cmd_fuse_start() {
    ensure_deps
    rm -rf "$MOUNT_DIR"; mkdir -p "$MOUNT_DIR"
    ./vfs_demo -f "$MOUNT_DIR" > /tmp/fuse_run.log 2>&1 &
    FPID=$!
    echo "$FPID" > /tmp/fuse_run.pid
    sleep 2
    echo "Mounted at $MOUNT_DIR (pid=$FPID)"
    ls -la "$MOUNT_DIR" || true
}

cmd_fuse_stop() {
    fusermount -u "$MOUNT_DIR" 2>/dev/null || true
    if [ -f /tmp/fuse_run.pid ]; then
        FPID=$(cat /tmp/fuse_run.pid)
        kill "$FPID" 2>/dev/null || true
        rm -f /tmp/fuse_run.pid
    fi
    echo "Unmounted $MOUNT_DIR"
}

cmd_fuse_status() {
    if mountpoint -q "$MOUNT_DIR"; then
        echo "Mounted: $MOUNT_DIR"
    else
        echo "Not mounted: $MOUNT_DIR"
    fi
}

cmd_fuse_shell() {
    if ! mountpoint -q "$MOUNT_DIR"; then
        echo "Not mounted. Starting..."
        cmd_fuse_start
    fi
    echo "Entering shell at $MOUNT_DIR. Type 'exit' to leave."
    (cd "$MOUNT_DIR" && bash --noprofile --norc)
}

cmd_demo() {
    cmd_build
    cmd_fuse_start
    echo "\nRoot listing:"; ls -la "$MOUNT_DIR"
    echo "\nNested listing:"; ls -la "$MOUNT_DIR/dir1" || true
    cmd_fuse_stop
}

case "${1:-}" in
    build)        cmd_build ;;
    test)         cmd_test ;;
    valgrind)     cmd_valgrind ;;
    fuse:start)   cmd_fuse_start ;;
    fuse:stop)    cmd_fuse_stop ;;
    fuse:status)  cmd_fuse_status ;;
    fuse:shell)   cmd_fuse_shell ;;
    demo)         cmd_demo ;;
    *)
        echo "Usage: $0 {build|test|valgrind|fuse:start|fuse:stop|fuse:status|fuse:shell|demo}"
        exit 1
        ;;
esac
