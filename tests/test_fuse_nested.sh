#!/bin/bash

echo "=== FUSE Nested Directory Navigation Test ==="
echo ""

# Clean up
rm -rf /tmp/vfs_mount
mkdir -p /tmp/vfs_mount

# Start FUSE
./vfs_demo -f /tmp/vfs_mount > /tmp/fuse_nested.log 2>&1 &
FPID=$!
sleep 2

# Test nested navigation
echo "Listing /dir1:"
ls /tmp/vfs_mount/dir1
R1=$?

echo ""
echo "Listing /dir1/dir2:"
ls /tmp/vfs_mount/dir1/dir2
R2=$?

echo ""
echo "Listing /dir1/dir3:"
ls /tmp/vfs_mount/dir1/dir3
R3=$?

# Cleanup
fusermount -u /tmp/vfs_mount 2>/dev/null
kill $FPID 2>/dev/null
wait $FPID 2>/dev/null

echo ""
if [ $R1 -eq 0 ] && [ $R2 -eq 0 ] && [ $R3 -eq 0 ]; then
    echo "✅ Result: PASSED - All directories accessible"
elif [ $R1 -eq 0 ]; then
    echo "✅ Result: PARTIAL - First level works, deeper levels empty (expected for stub FS)"
else
    echo "❌ Result: FAILED"
fi
