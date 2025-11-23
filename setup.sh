# --- 2. Create Folder Structure ---
echo "[3/8] Creating folder structure..."
mkdir -p src/core
mkdir -p src/fuse
mkdir -p src/backends
mkdir -p src/tools
mkdir -p tests
mkdir -p docs

# --- 4. Create Empty Source Files ---
echo "[5/8] Creating empty C source modules..."
touch src/core/vfs_core.c
touch src/fuse/vfs_fuse.c
touch src/backends/backend_posix.c
touch src/tools/vfsctl.c


