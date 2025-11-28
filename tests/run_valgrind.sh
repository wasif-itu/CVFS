#!/bin/bash
# Valgrind Memory Leak Detection Tests for VFS

set -e

echo "======================================"
echo "  VFS Valgrind Memory Leak Tests"
echo "======================================"
echo ""

# Check if valgrind is installed
if ! command -v valgrind &> /dev/null; then
    echo "ERROR: valgrind is not installed"
    echo "Install with: sudo apt-get install valgrind"
    exit 1
fi

echo "Running valgrind on all test suites..."
echo ""

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

VALGRIND_OPTS="--leak-check=full --show-leak-kinds=all --track-origins=yes --verbose --error-exitcode=1"
FAILED=0

# Test 1: Core Structs
echo "----------------------------------------"
echo "Test 1: test_core_structs"
echo "----------------------------------------"
if valgrind $VALGRIND_OPTS --log-file=valgrind_core.log ./tests/test_core_structs/test_core_structs > /dev/null 2>&1; then
    echo -e "${GREEN}✓ PASSED${NC} - No memory leaks detected"
else
    echo -e "${RED}✗ FAILED${NC} - Memory leaks detected! See valgrind_core.log"
    FAILED=$((FAILED+1))
fi
echo ""

# Test 2: Lookup
echo "----------------------------------------"
echo "Test 2: test_lookup"
echo "----------------------------------------"
if valgrind $VALGRIND_OPTS --log-file=valgrind_lookup.log ./test_lookup > /dev/null 2>&1; then
    echo -e "${GREEN}✓ PASSED${NC} - No memory leaks detected"
else
    echo -e "${RED}✗ FAILED${NC} - Memory leaks detected! See valgrind_lookup.log"
    FAILED=$((FAILED+1))
fi
echo ""

# Test 3: File Operations
echo "----------------------------------------"
echo "Test 3: test_file_ops"
echo "----------------------------------------"
if valgrind $VALGRIND_OPTS --log-file=valgrind_fileops.log ./test_file_ops > /dev/null 2>&1; then
    echo -e "${GREEN}✓ PASSED${NC} - No memory leaks detected"
else
    echo -e "${RED}✗ FAILED${NC} - Memory leaks detected! See valgrind_fileops.log"
    FAILED=$((FAILED+1))
fi
echo ""

# Test 4: Integration
echo "----------------------------------------"
echo "Test 4: test_integration"
echo "----------------------------------------"
if valgrind $VALGRIND_OPTS --log-file=valgrind_integration.log ./test_integration > /dev/null 2>&1; then
    echo -e "${GREEN}✓ PASSED${NC} - No memory leaks detected"
else
    echo -e "${RED}✗ FAILED${NC} - Memory leaks detected! See valgrind_integration.log"
    FAILED=$((FAILED+1))
fi
echo ""

# Summary
echo "======================================"
echo "  Summary"
echo "======================================"
if [ $FAILED -eq 0 ]; then
    echo -e "${GREEN}All tests passed!${NC} No memory leaks detected."
    echo ""
    echo "Cleaning up log files..."
    rm -f valgrind_*.log
    exit 0
else
    echo -e "${RED}$FAILED test(s) failed!${NC} Memory leaks detected."
    echo ""
    echo "Check the following log files for details:"
    ls -1 valgrind_*.log 2>/dev/null
    echo ""
    echo "To view a log: cat valgrind_<test>.log"
    exit 1
fi
