#!/bin/bash

echo "=== FUSE Mount Stability Test ==="
echo ""

# Clean up
rm -rf /tmp/vfs_mount
mkdir -p /tmp/vfs_mount

# Start FUSE
echo "Starting FUSE mount..."
./vfs_demo -f /tmp/vfs_mount > /tmp/fuse_stability.log 2>&1 &
FPID=$!
sleep 2

# Verify mount
echo "Verifying mount is active..."
if mountpoint -q /tmp/vfs_mount; then
    echo "✓ Mount active"
else
    echo "✗ Mount failed"
    kill $FPID 2>/dev/null
    exit 1
fi

echo ""
echo "Running 10 directory listings..."
PASS=0
for i in {1..10}; do
    if ls /tmp/vfs_mount > /dev/null 2>&1; then
        echo "  Iteration $i: OK"
        PASS=$((PASS + 1))
    else
        echo "  Iteration $i: FAILED"
    fi
done

echo ""
echo "Unmounting..."
fusermount -u /tmp/vfs_mount
if [ $? -eq 0 ]; then
    echo "✓ Clean unmount"
else
    echo "✗ Unmount had issues"
fi

kill $FPID 2>/dev/null
wait $FPID 2>/dev/null

echo ""
echo "Stability test: $PASS/10 operations successful"
if [ $PASS -eq 10 ]; then
    echo "✅ Result: PASSED"
else
    echo "⚠ Result: PARTIAL ($PASS/10 successful)"
fi
