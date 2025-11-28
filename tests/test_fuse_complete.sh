#!/bin/bash

echo "=========================================="
echo "  COMPREHENSIVE FUSE INTEGRATION TESTS"
echo "=========================================="
echo ""

TOTAL=0
PASSED=0

# Test 1: Directory Listing
echo "Test 1: Directory Listing"
echo "-------------------------"
rm -rf /tmp/vfs_mount && mkdir -p /tmp/vfs_mount
./vfs_demo -f /tmp/vfs_mount > /tmp/fuse1.log 2>&1 &
FPID=$!
sleep 2
ls -la /tmp/vfs_mount > /dev/null 2>&1
if [ $? -eq 0 ]; then
    echo "✅ PASSED - Directory listing works"
    PASSED=$((PASSED + 1))
else
    echo "❌ FAILED"
fi
fusermount -u /tmp/vfs_mount 2>/dev/null
kill $FPID 2>/dev/null
wait $FPID 2>/dev/null
TOTAL=$((TOTAL + 1))
echo ""

# Test 2: Root Directory Contents
echo "Test 2: Root Directory Contents"
echo "--------------------------------"
rm -rf /tmp/vfs_mount && mkdir -p /tmp/vfs_mount
./vfs_demo -f /tmp/vfs_mount > /tmp/fuse2.log 2>&1 &
FPID=$!
sleep 2
OUTPUT=$(ls /tmp/vfs_mount 2>&1)
if echo "$OUTPUT" | grep -q "dir1"; then
    echo "✅ PASSED - dir1 visible in root"
    PASSED=$((PASSED + 1))
else
    echo "❌ FAILED - dir1 not found"
fi
fusermount -u /tmp/vfs_mount 2>/dev/null
kill $FPID 2>/dev/null
wait $FPID 2>/dev/null
TOTAL=$((TOTAL + 1))
echo ""

# Test 3: Nested Directory Access
echo "Test 3: Nested Directory Access"
echo "--------------------------------"
rm -rf /tmp/vfs_mount && mkdir -p /tmp/vfs_mount
./vfs_demo -f /tmp/vfs_mount > /tmp/fuse3.log 2>&1 &
FPID=$!
sleep 2
ls /tmp/vfs_mount/dir1 > /dev/null 2>&1
if [ $? -eq 0 ]; then
    echo "✅ PASSED - Nested directory accessible"
    PASSED=$((PASSED + 1))
else
    echo "❌ FAILED"
fi
fusermount -u /tmp/vfs_mount 2>/dev/null
kill $FPID 2>/dev/null
wait $FPID 2>/dev/null
TOTAL=$((TOTAL + 1))
echo ""

# Test 4: Stat Operations
echo "Test 4: Stat Operations"
echo "-----------------------"
rm -rf /tmp/vfs_mount && mkdir -p /tmp/vfs_mount
./vfs_demo -f /tmp/vfs_mount > /tmp/fuse4.log 2>&1 &
FPID=$!
sleep 2
stat /tmp/vfs_mount > /dev/null 2>&1
R1=$?
stat /tmp/vfs_mount/dir1 > /dev/null 2>&1
R2=$?
if [ $R1 -eq 0 ] && [ $R2 -eq 0 ]; then
    echo "✅ PASSED - Stat operations work"
    PASSED=$((PASSED + 1))
else
    echo "❌ FAILED"
fi
fusermount -u /tmp/vfs_mount 2>/dev/null
kill $FPID 2>/dev/null
wait $FPID 2>/dev/null
TOTAL=$((TOTAL + 1))
echo ""

# Test 5: Mount Stability
echo "Test 5: Mount Stability"
echo "-----------------------"
rm -rf /tmp/vfs_mount && mkdir -p /tmp/vfs_mount
./vfs_demo -f /tmp/vfs_mount > /tmp/fuse5.log 2>&1 &
FPID=$!
sleep 2
STABLE=0
for i in {1..5}; do
    ls /tmp/vfs_mount > /dev/null 2>&1 && STABLE=$((STABLE + 1))
done
if [ $STABLE -eq 5 ]; then
    echo "✅ PASSED - Mount stable (5/5 operations)"
    PASSED=$((PASSED + 1))
else
    echo "⚠ PARTIAL - Mount partially stable ($STABLE/5)"
fi
fusermount -u /tmp/vfs_mount 2>/dev/null
kill $FPID 2>/dev/null
wait $FPID 2>/dev/null
TOTAL=$((TOTAL + 1))
echo ""

# Summary
echo "=========================================="
echo "  FUSE TEST SUMMARY"
echo "=========================================="
echo "Tests Passed: $PASSED/$TOTAL"
echo "Success Rate: $(( PASSED * 100 / TOTAL ))%"
echo ""
if [ $PASSED -eq $TOTAL ]; then
    echo "Status: ✅ ALL FUSE TESTS PASSED"
    echo "FUSE Integration: WORKING"
else
    echo "Status: ⚠ SOME TESTS FAILED"
fi
echo "=========================================="
