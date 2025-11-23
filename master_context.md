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

Calls VFS Core API

Handles init/destroy/open/read/write/getattr/readdir

Folder:

src/fuse/

3. Backend Layer (Person C)

Real filesystem drivers (POSIX driver first)

Each backend implements its own open/read/write/stat/readdir

Folder:

src/backends/

BUILD SYSTEM / TOOLCHAIN

Using WSL2 Ubuntu

Compiler: GCC (with FUSE3, sqlite3, pthreads)

Project root Makefile builds:

vfs_core.c

vfs_fuse.c

backend_posix.c

vfsctl tool

FOLDER STRUCTURE
vfs-project/
├─ src/
│  ├─ core/
│  ├─ fuse/
│  ├─ backends/
│  ├─ tools/
├─ tests/
├─ docs/
└─ Makefile

🔵 PERSON A – YOUR MODULE (VFS Core Layer)
RESPONSIBILITIES

You implement the entire in-memory VFS Core:

DATA STRUCTURES

vfs_inode_t

ino

mode

uid/gid

size

refcount

ptr to backend file handle info

vfs_dentry_t

name

parent

inode*

lock

children list / map

backend info (backend_id + relpath)

mount_entry_t

backend_id

mount path (e.g., /mnt1)

root dentry for that backend

Core global tables:

mount table

inode table

dentry root

CORE PUBLIC API YOU MUST IMPLEMENT

Defined in vfs_core.h:

int vfs_init();
int vfs_shutdown();

int vfs_mount_backend(const char *name, const char *rootpath, const char *type);
int vfs_unmount_backend(const char *name);

int vfs_lookup(const char *path, vfs_inode_t **out);

int vfs_open(const char *path, int flags);
ssize_t vfs_read(int fh, void *buf, size_t count, off_t offset);
ssize_t vfs_write(int fh, const void *buf, size_t count, off_t offset);

int vfs_stat(const char *path, struct stat *st);
int vfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler);

int vfs_permission_check(const char *path, uid_t uid, gid_t gid, int mask);

YOUR MILESTONES
Day 1

Write vfs_core.h

Write stub implementations in vfs_core.c

Day 2

Implement core structures (inode/dentry)

Implement creation/destruction with refcounting

Day 3

Implement path resolution (vfs_resolve_path → vfs_lookup)

Day 4

Connect lookup with backend translation

Backend relpath extraction

Day 5–8

File handle table

open/read/write/stat/readdir bridging to backend

Caching

Permission checks

Error handling

Day 9–10

Integration testing with FUSE + backend

🔶 STANDARD WAY TO SEND UPDATED FUNCTIONS

Whenever you change or rewrite a function, use this exact format:

UPDATE BLOCK (for updated functions)

MODULE: src/core/vfs_core.c
FUNCTION: vfs_lookup()
NEW VERSION:

// your updated function code here


NOTES:

What changed

What problem this fixes

Dependencies (if any)

This keeps everything stable even if ChatGPT forgets earlier messages.

🔴 STANDARD DEBUGGING REQUEST FORMAT

If something breaks in any layer, send a full structured snapshot:

DEBUG REQUEST

Error:
(copy/paste compiler error, runtime error, or behavior)

Relevant Function(s):

file.c / function name

file2.c / function name

Context:

What you were trying to do

What you expected

What happened instead

Full Code Block:

// include all relevant functions here


Master Context File Attached:
(yes/no)

If you include the Master Context File, I can reconstruct the entire mental model instantly.

🔵 OPTIONAL: MASTER CONTEXT SHORT VERSION

If you want a lightweight version for quick reminders, here:

SHORT CONTEXT VERSION

We are building a 3-layer VFS in C:

VFS Core (you)

mount table, path resolve, inode+ dentry cache, API

vfs_init/lookup/open/read/write/stat/readdir

FUSE Layer

FUSE callbacks → call VFS Core API

Backend Layer

POSIX driver first

open/read/write/stat/readdir via actual Linux FS

Core uses in-memory dentry+inode tree.
Backend uses relpath inside a mounted backend.
Everything is compiled with a shared Makefile.