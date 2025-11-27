✅ MASTER CONTEXT FILE (Person A – VFS Core Layer)

→ Copy-paste this entire block into ChatGPT whenever context is lost.
→ You can also extend it as the project evolves.

MASTER CONTEXT – FUSE-BASED VFS PROJECT (Person A: VFS Core Layer)
PROJECT OVERVIEW

We are building a Layered Architecture Virtual Filesystem (VFS) with FUSE3.

The project contains three layers:

1. VFS Core Layer (Person A)

The main responsibilities are:

Mount table (mounting/unmounting backends)

Path resolution

Dentry/inode cache

Permission checks

Public API used by both FUSE and backend drivers

File handle table

Memory-only VFS metadata tree

Folder:

src/core/
    vfs_core.h
    vfs_core.c

2. FUSE Layer (Person B)

Implements FUSE callbacks

✅ MASTER CONTEXT FILE (Person A – VFS Core Layer)

This file is the authoritative per-person-A context snapshot (as of 2025-11-24).
Copy this block into an AI prompt to restore Person A's mental model quickly.

-- SUMMARY (Person A current status)

- Work completed:
    - Implemented in-memory VFS core prototypes and a working clean implementation in `src/core/vfs_core.c` and header `src/core/vfs_core.h`.
    - Implemented inode & dentry lifecycle: creation, reference counting, acquire/release.
    - Implemented mount table helpers and a simple default root mount.
    - Implemented path normalization and `vfs_resolve_path()` that locates mount points and walks/auto-creates dentries in-memory.
    - Implemented helper functions used by tests: `vfs_dentry_add_child`, `vfs_dentry_remove_child`, `vfs_dentry_destroy`, `vfs_dentry_destroy_tree`, and `vfs_dentry_release` (thin wrapper).
    - Added a minimal CLI `src/tools/vfsctl.c` (main) used during builds.
    - Tests passing locally (WSL): `tests/test_core_structs` and `tests/test_lookup` both run and pass on current workspace.

-- Files touched (core area)

- `src/core/vfs_core.h` — public header (types + prototypes used by tests and other layers).
- `src/core/vfs_core.c` — current canonical implementation (in-memory core used by tests).
- `src/tools/vfsctl.c` — small CLI with `main()` that calls `vfs_init()` / `vfs_shutdown()`.
- Tests referencing core:
    - `tests/test_core_structs/test_core_structs.c`
    - `tests/test_lookup.c`

-- Implemented functions (inventory)

From `src/core/vfs_core.c` and available to callers (or present in file):

- Initialization / shutdown:
    - `int vfs_init(void)`
    - `int vfs_shutdown(void)`

- Mount helpers:
    - `vfs_mount_entry_t *vfs_mount_create(const char *mountpoint, const char *backend_root)`
    - `int vfs_mount_destroy(vfs_mount_entry_t *m)`
    - static helper: `static vfs_mount_entry_t *find_best_mount(const char *path)`

- Path normalization / resolution:
    - `static char *normalize_path_alloc(const char *path)`
    - `int vfs_resolve_path(const char *path, vfs_dentry_t **out)`

- Inode helpers:
    - `vfs_inode_t *vfs_inode_create(uint64_t ino, mode_t mode, uid_t uid, gid_t gid, off_t size)`
    - `void vfs_inode_acquire(vfs_inode_t *ino)`
    - `void vfs_inode_release(vfs_inode_t *ino)`

- Dentry helpers:
    - `vfs_dentry_t *vfs_dentry_create(const char *name, vfs_dentry_t *parent, vfs_inode_t *inode)`
    - `void vfs_dentry_add_child(vfs_dentry_t *parent, vfs_dentry_t *child)`
    - `void vfs_dentry_remove_child(vfs_dentry_t *parent, vfs_dentry_t *child)`
    - `void vfs_dentry_destroy(vfs_dentry_t *dentry)`
    - `void vfs_dentry_destroy_tree(vfs_dentry_t *root)`
    - `void vfs_dentry_release(vfs_inode_t *inode)` — thin wrapper around `vfs_inode_release` used by test helper code

-- Global state (in `vfs_core.c`)

- `static pthread_mutex_t g_vfs_lock` — global VFS lock
- `vfs_mount_entry_t *mount_table_head` — exported mount list head
- `static uint64_t g_next_ino` — ino counter
- `static int g_vfs_inited` — init flag

-- Tests & results (local)

- `tests/test_core_structs` — exercises inode refcount and dentry create/destroy; PASSED.
- `tests/test_lookup` — exercises `vfs_resolve_path` (normalization and auto-create behavior); PASSED.

-- Known API surface still missing or TODO for Person A

These are the recommended next items (short-term priorities):

1) Public API completion / wrappers:
     - Implement `vfs_lookup()` public wrapper (currently header declares it; the resolver implemented is `vfs_resolve_path`). Add any missing wrappers expected by other layers.
     - Provide compatibility aliases if other code/tests expect different type names (for example, `typedef vfs_mount_entry_t mount_entry_t;`) to avoid mismatch.

2) File handle table & open/read/write/stat/readdir APIs:
     - `int vfs_open(const char *path, int flags)`
     - `ssize_t vfs_read(int fh, void *buf, size_t count, off_t offset)`
     - `ssize_t vfs_write(int fh, const void *buf, size_t count, off_t offset)`
    - `int vfs_stat(const char *path, struct stat *st)`
    - `int vfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler)`

3) Backend integration hooks and relpath extraction:
     - Extend dentry/inode records with backend id & relative path fields (or return a small struct with backend id + relpath) used by backends.

4) Permission checks & inode-level locks:
     - Add `vfs_permission_check()` and per-inode `pthread_rwlock_t` for read/write locking.

5) Caching & file handle lifecycle:
     - Simple page cache module and integration with `vfs_read`/`vfs_write`.
     - Write-through semantics initially; later LRU/TTL.

6) Stability & cleanup:
     - Consolidate single canonical `vfs_core.c` (remove other variants like `vfs_core_clean.c` if present) and ensure Makefile compiles only that file.
     - Add detailed unit tests and stress tests for concurrent lookups and create/unlink operations.

-- Suggested immediate developer actions (Person A)

- Convert `vfs_resolve_path` → add small public `vfs_lookup` wrapper that preserves header API.
- Add `mount_entry_t` typedef alias if other code expects that name.
- Add stubs for `vfs_open/vfs_read/vfs_write/vfs_stat/vfs_readdir/vfs_permission_check` in `vfs_core.c` returning `-ENOSYS` so Person B/C can compile against them and implement incrementally.
- Run `make test` and `make` after changes.

-- Project structure snapshot (full)

Top-level files and folders (current workspace):

`- `Makefile` — project build rules
`- `master_context.md` — this file
`- `setup.sh` — project setup helper
`- `docs/` — documentation folder
`- `src/`
    - `core/`
        - `vfs_core.h`  (public header)
        - `vfs_core.c`  (canonical implementation used by tests)
    - `fuse/`
        - `vfs_fuse.c`  (FUSE glue; may be skeleton/stubs)
    - `backends/`
        - `backend_posix.c` (POSIX backend adapter stub)
    - `tools/`
        - `vfsctl.c` (small CLI main)
`- `tests/`
    - `test_core_structs/`
        - `test_core_structs.c`
    - `test_lookup.c`

-- Quick commands you can run locally (WSL)

```bash
# build everything
make

# run unit tests
make test
```

-- Notes for AI maintainers

- This file is the single-source summary an AI should read to work on Person A tasks.
- When generating code, produce small, focused patches that add stubs first, then implement behavior; keep header stable.

-- Change log (recent edits by developer agent)

- 2025-11-23..24: Implemented `vfs_core.c` core helpers, added `vfs_dentry_*` helpers and `vfs_resolve_path()`, added `vfs_dentry_release` wrapper and `vfsctl.c` main to satisfy build/tests. Tests `test_core_structs` and `test_lookup` pass locally.

If you want, I can now (pick one):
- add stable stubs for the remaining public API (`vfs_open`, `vfs_read`, `vfs_write`, `vfs_stat`, `vfs_readdir`, `vfs_permission_check`) and run `make test` again; OR
- create the design doc `docs/design.md` for Person A (data structures, locking, API surface). Reply with which you prefer and I'll proceed.

---

**Person A Status vs 10-Day Plan**

- Before Day 1 — Shared Setup: Done (repo + Makefile exist).

- Day 1 — Headers and Stubs: Done
    - `src/core/vfs_core.h` defines public API.
    - `src/core/vfs_core.c` compiles; stubs added and later implemented.

- Day 2 — Core Data Structures: Done
    - Implemented `vfs_inode_t`, `vfs_dentry_t`, refcounting, create/destroy helpers.
    - Unit test `tests/test_core_structs` passes.

- Day 3 — Path Resolution: Done
    - Implemented `vfs_resolve_path` (normalization + mount selection + tree walk/auto-create).
    - Added `vfs_lookup` wrapper.
    - `tests/test_lookup` passes.

- Day 4 — Mount Table Implementation: ✅ Done
    - Implemented `vfs_mount_entry_t` plus `vfs_mount_create`/`vfs_mount_destroy`.
    - Default root mount created in `vfs_init`.
    - Public API `vfs_mount_backend`/`vfs_unmount_backend` implemented as wrappers around internal mount helpers.

- Day 5 — Backend Dispatch Layer: ✅ Done (Design Phase)
    - Designed `vfs_backend_ops_t` function table structure with init/shutdown and file operation callbacks.
    - Updated `vfs_mount_entry_t` to include `backend_ops` and `backend_data` pointers.
    - Mount lifecycle now calls backend shutdown if backend present.
    - TODO: Implement actual backends (Person C) and dispatch file operations to backends when present.

- Day 6 — Permission Model: ✅ Done
    - Implemented `vfs_permission_check` with UNIX-style owner/group/other evaluation (R_OK/W_OK/X_OK).
    - Integrated permission checks into `vfs_open`, `vfs_read`, `vfs_write`.
    - Root (uid=0) bypasses read/write permission checks.

- Day 7 — Caching Layer: Not Started
    - No dentry/inode cache beyond in-memory tree; no LRU/TTL.

- Day 8 — File Handles + Core Read/Write: ✅ Done
    - Implemented global file handle table (1024 entries) with thread-safe allocation/deallocation.
    - `vfs_open` with permission checks and EISDIR rejection for directories.
    - `vfs_close` releases file handles.
    - `vfs_read` returns zero-filled data up to inode size.
    - `vfs_write` extends inode size (no actual content storage until backend integration).
    - Unit tests in `tests/test_file_ops.c` validate error handling (EISDIR, EBADF).

- Day 9 — Directory Operations: ✅ Done
    - Implemented `vfs_stat` fills struct stat from inode metadata.
    - Implemented `vfs_readdir` iterates child dentries and calls filler callback.
    - Integration tested through `tests/test_file_ops.c`.

- Day 10 — Final Integration & Refactoring: Pending (Integration Phase)
    - Backend dispatch interface designed; awaiting Person C backend implementation.
    - FUSE layer (Person B) can use public VFS API.
    - Pending: valgrind memory leak checks, stress testing, concurrent operation validation.

**Immediate TODOs (Person A - Remaining Work)**

- ✅ Day 4: `vfs_mount_backend`/`vfs_unmount_backend` - COMPLETED
- ✅ Day 5: Backend dispatch interface design - COMPLETED
- ✅ Day 6: Permission checks implementation - COMPLETED
- ✅ Day 8-9: File operations and directory APIs - COMPLETED
- 🟡 Day 7: Caching layer (optional) - Deferred (no hash table, no LRU yet)
- 🔄 Day 10: Integration work - In Progress
    - ⏳ Person C: Implement backends (backend_posix.c) with vfs_backend_ops_t structure
    - ⏳ Person C: Update file operations to dispatch to backend callbacks when present
    - ⏳ Person B: Integrate FUSE callbacks with VFS public API
    - ⏳ Testing: End-to-end mount/open/read/write through FUSE
    - ⏳ Testing: Valgrind memory leak detection
    - ⏳ Testing: Stress test concurrent operations

**Current Test Status (All Passing)**

- ✅ `tests/test_core_structs` - Inode refcounting and dentry lifecycle
- ✅ `tests/test_lookup` - Path resolution with normalization
- ✅ `tests/test_file_ops` - File operations error handling