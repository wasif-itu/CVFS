#!/bin/bash

echo "=== FUSE Directory Listing Test ==="
echo ""

# Clean up
rm -rf /tmp/vfs_mount
mkdir -p /tmp/vfs_mount

# Start FUSE
./vfs_demo -f /tmp/vfs_mount > /tmp/fuse_test.log 2>&1 &
FPID=$!
sleep 2

# Test directory listing
echo "Root directory contents:"
ls -la /tmp/vfs_mount
RESULT=$?

# Cleanup
fusermount -u /tmp/vfs_mount 2>/dev/null
kill $FPID 2>/dev/null
wait $FPID 2>/dev/null

echo ""
if [ $RESULT -eq 0 ]; then
    echo "✅ Result: PASSED"
else
    echo "❌ Result: FAILED"
fi
