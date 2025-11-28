CC=gcc
CFLAGS=-Wall -Wextra -I./src -I./src/core -I./src/fuse -I./src/backends -pthread `pkg-config fuse3 --cflags`
LIBS=`pkg-config fuse3 --libs` -lsqlite3 -lpthread

# -----------------------------
# Source Files
# -----------------------------
CORE_SRC=src/core/vfs_core.c
FUSE_SRC=src/fuse/vfs_fuse.c
BACKEND_SRC=src/backends/backend_posix.c
TOOLS_SRC=src/tools/vfsctl.c

OBJ=$(CORE_SRC:.c=.o) $(FUSE_SRC:.c=.o) $(BACKEND_SRC:.c=.o) $(TOOLS_SRC:.c=.o)

# -----------------------------
# Default Target
# -----------------------------
all: vfs_demo

vfs_demo: $(OBJ)
	$(CC) -o $@ $^ $(LIBS)

# -----------------------------
# Clean
# -----------------------------
clean:
	rm -f $(OBJ) vfs_demo \
	      tests/test_core_structs/test_core_structs \
	      tests/test_lookup test_lookup.o \
	      tests/test_file_ops test_file_ops.o \
	      test_integration test_integration.o \
	      test_stress test_stress.o \
	      valgrind_*.log fuse_output.log

# -----------------------------
# Test: Core Structs
# -----------------------------
TEST_CORE_DIR=tests/test_core_structs
TEST_CORE_BIN=$(TEST_CORE_DIR)/test_core_structs
TEST_CORE_SRC=$(TEST_CORE_DIR)/test_core_structs.c src/core/vfs_core.c

.PHONY: test_core
test_core: $(TEST_CORE_BIN)
	./$(TEST_CORE_BIN)

$(TEST_CORE_BIN): $(TEST_CORE_SRC) $(BACKEND_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# -----------------------------
# Test: Lookup
# -----------------------------
TEST_LOOKUP_SRC=tests/test_lookup.c
TEST_LOOKUP_OBJ=$(TEST_LOOKUP_SRC:.c=.o)

.PHONY: test_lookup
test_lookup: $(TEST_LOOKUP_OBJ) $(CORE_SRC:.c=.o) $(BACKEND_SRC:.c=.o)
	$(CC) -o $@ $^ $(LIBS)
	./test_lookup

# -----------------------------
# Test: File Operations
# -----------------------------
TEST_FILE_OPS_SRC=tests/test_file_ops.c
TEST_FILE_OPS_OBJ=$(TEST_FILE_OPS_SRC:.c=.o)

.PHONY: test_file_ops
test_file_ops: $(TEST_FILE_OPS_OBJ) $(CORE_SRC:.c=.o) $(BACKEND_SRC:.c=.o)
	$(CC) -o $@ $^ $(LIBS)
	./test_file_ops

# -----------------------------
# Test: Integration (Backend + VFS + FUSE)
# -----------------------------
TEST_INTEGRATION_SRC=tests/test_integration.c
TEST_INTEGRATION_OBJ=$(TEST_INTEGRATION_SRC:.c=.o)

.PHONY: test_integration
test_integration: $(TEST_INTEGRATION_OBJ) $(CORE_SRC:.c=.o) $(BACKEND_SRC:.c=.o)
	$(CC) -o $@ $^ $(LIBS)
	./test_integration

# -----------------------------
# Test: Stress (Concurrent Operations)
# -----------------------------
TEST_STRESS_SRC=tests/test_stress.c
TEST_STRESS_OBJ=$(TEST_STRESS_SRC:.c=.o)

.PHONY: test_stress
test_stress: $(TEST_STRESS_OBJ) $(CORE_SRC:.c=.o) $(BACKEND_SRC:.c=.o)
	$(CC) -o $@ $^ $(LIBS)
	./test_stress

# -----------------------------
# Test: Valgrind (Memory Leak Detection)
# -----------------------------
.PHONY: test_valgrind
test_valgrind: test_core test_lookup test_file_ops test_integration
	@chmod +x tests/run_valgrind.sh
	@tests/run_valgrind.sh

# -----------------------------
# Test: FUSE Mount
# -----------------------------
.PHONY: test_fuse
test_fuse: vfs_demo
	@chmod +x tests/run_fuse_test.sh
	@tests/run_fuse_test.sh

# -----------------------------
# Run ALL tests (basic + stress)
# -----------------------------
.PHONY: test
test: test_core test_lookup test_file_ops test_integration test_stress

# -----------------------------
# Run ALL tests including valgrind and FUSE
# -----------------------------
.PHONY: test_all
test_all: test test_valgrind test_fuse
