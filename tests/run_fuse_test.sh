#!/bin/bash
# FUSE Mount Test Script

set -e

echo "======================================"
echo "  VFS FUSE Mount Integration Test"
echo "======================================"
echo ""

# Configuration
MOUNT_POINT="/tmp/vfs_mount"
BACKEND_DIR="/tmp/vfs_backend"
TEST_FILE="$MOUNT_POINT/test_fuse_file.txt"
TEST_DATA="Hello from FUSE mount! $(date)"

# Colors
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m'

# Cleanup function
cleanup() {
    echo ""
    echo "Cleaning up..."
    
    # Unmount if mounted
    if mountpoint -q "$MOUNT_POINT" 2>/dev/null; then
        echo "  - Unmounting FUSE filesystem..."
        fusermount -u "$MOUNT_POINT" 2>/dev/null || umount "$MOUNT_POINT" 2>/dev/null || true
        sleep 1
    fi
    
    # Kill any remaining vfs_demo processes
    pkill -9 vfs_demo 2>/dev/null || true
    
    echo "  - Cleanup complete"
}

# Set trap for cleanup
trap cleanup EXIT INT TERM

# Check if FUSE is available
if ! command -v fusermount &> /dev/null; then
    echo -e "${RED}ERROR:${NC} fusermount not found. Install with:"
    echo "  sudo apt-get install fuse3 libfuse3-dev"
    exit 1
fi

# Check if vfs_demo exists
if [ ! -f "./vfs_demo" ]; then
    echo -e "${RED}ERROR:${NC} vfs_demo not found. Build it first with: make"
    exit 1
fi

# Prepare directories
echo "1. Preparing directories..."
rm -rf "$MOUNT_POINT" "$BACKEND_DIR"
mkdir -p "$MOUNT_POINT" "$BACKEND_DIR"
echo -e "   ${GREEN}✓${NC} Directories ready"
echo ""

# Mount the filesystem
echo "2. Mounting FUSE filesystem..."
echo "   Command: ./vfs_demo -f $MOUNT_POINT"
echo "   (Running in background, will show logs)"
echo ""

# Start FUSE in foreground mode with debug output (background it)
./vfs_demo -f "$MOUNT_POINT" > fuse_output.log 2>&1 &
FUSE_PID=$!
echo "   FUSE process PID: $FUSE_PID"

# Wait for mount to complete
echo "   Waiting for mount..."
sleep 2

# Check if mounted
if ! mountpoint -q "$MOUNT_POINT" 2>/dev/null; then
    echo -e "   ${RED}✗ FAILED${NC} - Mount did not succeed"
    echo ""
    echo "FUSE output:"
    cat fuse_output.log
    kill $FUSE_PID 2>/dev/null || true
    exit 1
fi
echo -e "   ${GREEN}✓${NC} Filesystem mounted at $MOUNT_POINT"
echo ""

# Test 1: List directory
echo "3. Test: List root directory"
if ls -la "$MOUNT_POINT" > /dev/null 2>&1; then
    echo -e "   ${GREEN}✓${NC} Directory listing successful"
    ls -la "$MOUNT_POINT" | head -5
else
    echo -e "   ${RED}✗${NC} Directory listing failed"
    exit 1
fi
echo ""

# Test 2: Create file through FUSE
echo "4. Test: Create file through FUSE"
if echo "$TEST_DATA" > "$TEST_FILE" 2>/dev/null; then
    echo -e "   ${GREEN}✓${NC} File created: $TEST_FILE"
else
    echo -e "   ${RED}✗${NC} File creation failed"
    exit 1
fi
echo ""

# Test 3: Read file back
echo "5. Test: Read file back"
if [ -f "$TEST_FILE" ]; then
    READ_DATA=$(cat "$TEST_FILE" 2>/dev/null)
    if [ "$READ_DATA" == "$TEST_DATA" ]; then
        echo -e "   ${GREEN}✓${NC} File read successful, data matches"
        echo "   Content: $READ_DATA"
    else
        echo -e "   ${RED}✗${NC} File content mismatch"
        echo "   Expected: $TEST_DATA"
        echo "   Got:      $READ_DATA"
        exit 1
    fi
else
    echo -e "   ${RED}✗${NC} File not found"
    exit 1
fi
echo ""

# Test 4: File metadata
echo "6. Test: File metadata (stat)"
if stat "$TEST_FILE" > /dev/null 2>&1; then
    echo -e "   ${GREEN}✓${NC} File stat successful"
    stat "$TEST_FILE" | grep -E "(Size|Access|Modify)"
else
    echo -e "   ${RED}✗${NC} File stat failed"
    exit 1
fi
echo ""

# Test 5: Create directory
echo "7. Test: Create directory"
TEST_DIR="$MOUNT_POINT/test_directory"
if mkdir "$TEST_DIR" 2>/dev/null; then
    echo -e "   ${GREEN}✓${NC} Directory created: $TEST_DIR"
    ls -ld "$TEST_DIR"
else
    echo -e "   ${YELLOW}⚠${NC} Directory creation not supported (OK for basic mount)"
fi
echo ""

# Test 6: Multiple files
echo "8. Test: Multiple file operations"
for i in {1..5}; do
    if echo "Test file $i" > "$MOUNT_POINT/file_$i.txt" 2>/dev/null; then
        : # success
    else
        echo -e "   ${RED}✗${NC} Failed to create file_$i.txt"
        exit 1
    fi
done
echo -e "   ${GREEN}✓${NC} Created 5 files"
echo "   Files in mount:"
ls -1 "$MOUNT_POINT" | grep "file_" || echo "   (No pattern match)"
echo ""

# Check FUSE logs
echo "9. FUSE Operation Logs (last 20 lines):"
echo "----------------------------------------"
tail -20 fuse_output.log
echo "----------------------------------------"
echo ""

# Unmount
echo "10. Unmounting filesystem..."
fusermount -u "$MOUNT_POINT" 2>/dev/null || umount "$MOUNT_POINT" 2>/dev/null
sleep 1
kill $FUSE_PID 2>/dev/null || true
echo -e "    ${GREEN}✓${NC} Unmounted successfully"
echo ""

# Final summary
echo "======================================"
echo "  FUSE Integration Test Summary"
echo "======================================"
echo -e "${GREEN}✅ ALL FUSE TESTS PASSED${NC}"
echo ""
echo "The filesystem was successfully:"
echo "  • Mounted via FUSE"
echo "  • Accessible from userspace"
echo "  • File creation working"
echo "  • File read/write working"
echo "  • Metadata (stat) working"
echo "  • Cleanly unmounted"
echo ""
echo "Check fuse_output.log for detailed FUSE operations."
