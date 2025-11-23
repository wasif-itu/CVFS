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
	      tests/test_lookup test_lookup.o

# -----------------------------
# Test: Core Structs
# -----------------------------
TEST_CORE_DIR=tests/test_core_structs
TEST_CORE_BIN=$(TEST_CORE_DIR)/test_core_structs
TEST_CORE_SRC=$(TEST_CORE_DIR)/test_core_structs.c src/core/vfs_core.c

.PHONY: test_core
test_core: $(TEST_CORE_BIN)
	./$(TEST_CORE_BIN)

$(TEST_CORE_BIN): $(TEST_CORE_SRC)
	$(CC) $(CFLAGS) -o $@ $^ $(LIBS)

# -----------------------------
# Test: Lookup
# -----------------------------
TEST_LOOKUP_SRC=tests/test_lookup.c
TEST_LOOKUP_OBJ=$(TEST_LOOKUP_SRC:.c=.o)

.PHONY: test_lookup
test_lookup: $(TEST_LOOKUP_OBJ) $(CORE_SRC:.c=.o)
	$(CC) -o $@ $^ $(LIBS)
	./test_lookup

# -----------------------------
# Run ALL tests
# -----------------------------
.PHONY: test
test: test_core test_lookup
