# Day 10 Integration Plan - Detailed Implementation Guide

## Overview
This document provides step-by-step integration guidance with code snippets for connecting:
- **VFS Core** (vfs_core.c/h) - in-memory filesystem layer
- **Backend POSIX** (backend_posix.c/h) - actual file I/O operations
- **FUSE Layer** (vfs_fuse.c/h) - userspace filesystem interface

---

## STEP 1: Create Backend Ops Structure for POSIX Backend

### Goal
Wrap existing POSIX backend functions into `vfs_backend_ops_t` structure defined in vfs_core.h

### Current Issue
- POSIX backend uses signatures like: `int posix_open(int backend_id, const char *relpath, int flags, mode_t mode)`
- VFS expects: `int (*open)(void *backend_data, const char *relpath, int flags, void **handle)`

### Solution: Create Adapter Functions

**File: `src/backends/backend_posix.c`**

Add these adapter functions at the end of the file:

```c
/* ========================================================================
 * VFS Backend Ops Adapters
 * ======================================================================== */

/* Adapter: init - wraps posix_backend_init */
static int posix_ops_init(const char *root_path, void **backend_data) {
    if (!backend_data) return -EINVAL;
    
    int backend_id = posix_backend_init(root_path);
    if (backend_id < 0) return -errno;
    
    /* Store backend_id as opaque pointer */
    *backend_data = (void *)(intptr_t)backend_id;
    return 0;
}

/* Adapter: shutdown - wraps posix_backend_shutdown */
static int posix_ops_shutdown(void *backend_data) {
    if (!backend_data) return -EINVAL;
    
    int backend_id = (int)(intptr_t)backend_data;
    return posix_backend_shutdown(backend_id);
}

/* Adapter: open - wraps posix_open */
static int posix_ops_open(void *backend_data, const char *relpath, int flags, void **handle) {
    if (!backend_data || !handle) return -EINVAL;
    
    int backend_id = (int)(intptr_t)backend_data;
    /* posix_open expects mode for O_CREAT, use default 0644 */
    mode_t mode = 0644;
    
    int h = posix_open(backend_id, relpath, flags, mode);
    if (h < 0) return -errno;
    
    *handle = (void *)(intptr_t)h;
    return 0;
}

/* Adapter: close - wraps posix_close */
static int posix_ops_close(void *backend_data, void *handle) {
    if (!backend_data || !handle) return -EINVAL;
    
    int backend_id = (int)(intptr_t)backend_data;
    int h = (int)(intptr_t)handle;
    
    int ret = posix_close(backend_id, h);
    return (ret < 0) ? -errno : 0;
}

/* Adapter: read - wraps posix_read */
static ssize_t posix_ops_read(void *backend_data, void *handle, void *buf, 
                               size_t count, off_t offset) {
    if (!backend_data || !handle || !buf) return -EINVAL;
    
    int backend_id = (int)(intptr_t)backend_data;
    int h = (int)(intptr_t)handle;
    
    ssize_t ret = posix_read(backend_id, h, buf, count, offset);
    return (ret < 0) ? -errno : ret;
}

/* Adapter: write - wraps posix_write */
static ssize_t posix_ops_write(void *backend_data, void *handle, const void *buf,
                                size_t count, off_t offset) {
    if (!backend_data || !handle || !buf) return -EINVAL;
    
    int backend_id = (int)(intptr_t)backend_data;
    int h = (int)(intptr_t)handle;
    
    ssize_t ret = posix_write(backend_id, h, buf, count, offset);
    return (ret < 0) ? -errno : ret;
}

/* Adapter: stat - wraps posix_stat */
static int posix_ops_stat(void *backend_data, const char *relpath, struct stat *st) {
    if (!backend_data || !relpath || !st) return -EINVAL;
    
    int backend_id = (int)(intptr_t)backend_data;
    int ret = posix_stat(backend_id, relpath, st);
    return (ret < 0) ? -errno : 0;
}

/* Adapter: readdir - wraps posix_readdir */
static int posix_ops_readdir(void *backend_data, const char *relpath, 
                              void *buf, void *filler) {
    if (!backend_data || !relpath || !filler) return -EINVAL;
    
    int backend_id = (int)(intptr_t)backend_data;
    /* Cast filler to vfs_fill_dir_t (matches fuse_fill_dir_t signature) */
    vfs_fill_dir_t fill_fn = (vfs_fill_dir_t)filler;
    
    int ret = posix_readdir(backend_id, relpath, buf, fill_fn, 0);
    return (ret < 0) ? -errno : 0;
}

/* Global backend ops structure */
const vfs_backend_ops_t posix_backend_ops = {
    .name = "posix",
    .init = posix_ops_init,
    .shutdown = posix_ops_shutdown,
    .open = posix_ops_open,
    .close = posix_ops_close,
    .read = posix_ops_read,
    .write = posix_ops_write,
    .stat = posix_ops_stat,
    .readdir = posix_ops_readdir,
};

/* Getter function for backend ops */
const vfs_backend_ops_t *get_posix_backend_ops(void) {
    return &posix_backend_ops;
}
```

**File: `src/backends/backend_posix.h`**

Add at the end:

```c
/* Get the VFS backend ops structure for POSIX backend */
const struct vfs_backend_ops *get_posix_backend_ops(void);
```

Also add forward declaration at the top after includes:

```c
/* Forward declaration for VFS integration */
struct vfs_backend_ops;
```

---

## STEP 2: Implement Backend Registry in VFS Core

### Goal
Allow backends to register themselves and be looked up by name

### Implementation

**File: `src/core/vfs_core.c`**

Add after global state section (around line 25):

```c
/* -------------------------------------------------------------------------- */
/* BACKEND REGISTRY */
/* -------------------------------------------------------------------------- */
#define MAX_BACKENDS 8

static struct {
    const vfs_backend_ops_t *ops;
    int registered;
} g_backend_registry[MAX_BACKENDS];

static pthread_mutex_t g_backend_lock = PTHREAD_MUTEX_INITIALIZER;

/* Register a backend operations structure */
int vfs_register_backend(const vfs_backend_ops_t *ops) {
    if (!ops || !ops->name) return -EINVAL;
    
    pthread_mutex_lock(&g_backend_lock);
    
    /* Check if already registered */
    for (int i = 0; i < MAX_BACKENDS; i++) {
        if (g_backend_registry[i].registered && 
            strcmp(g_backend_registry[i].ops->name, ops->name) == 0) {
            pthread_mutex_unlock(&g_backend_lock);
            return -EEXIST;
        }
    }
    
    /* Find free slot */
    for (int i = 0; i < MAX_BACKENDS; i++) {
        if (!g_backend_registry[i].registered) {
            g_backend_registry[i].ops = ops;
            g_backend_registry[i].registered = 1;
            pthread_mutex_unlock(&g_backend_lock);
            return 0;
        }
    }
    
    pthread_mutex_unlock(&g_backend_lock);
    return -ENOMEM;
}

/* Lookup backend by name */
static const vfs_backend_ops_t *vfs_find_backend(const char *name) {
    if (!name) return NULL;
    
    pthread_mutex_lock(&g_backend_lock);
    for (int i = 0; i < MAX_BACKENDS; i++) {
        if (g_backend_registry[i].registered && 
            strcmp(g_backend_registry[i].ops->name, name) == 0) {
            const vfs_backend_ops_t *ops = g_backend_registry[i].ops;
            pthread_mutex_unlock(&g_backend_lock);
            return ops;
        }
    }
    pthread_mutex_unlock(&g_backend_lock);
    return NULL;
}
```

**File: `src/core/vfs_core.h`**

Add after vfs_unmount_backend declaration:

```c
/* Register a backend with the VFS */
int vfs_register_backend(const vfs_backend_ops_t *ops);
```

Update `vfs_mount_backend` implementation in vfs_core.c (replace existing TODO):

```c
int vfs_mount_backend(const char *mountpoint, const char *backend_root,
                      const char *backend_type)
{
    if (!mountpoint || !backend_root || !backend_type)
        return -EINVAL;

    /* Lookup registered backend */
    const vfs_backend_ops_t *ops = vfs_find_backend(backend_type);
    if (!ops) {
        fprintf(stderr, "vfs_mount_backend: backend '%s' not registered\n", backend_type);
        return -ENODEV;
    }

    /* Create mount structure */
    vfs_mount_entry_t *m = vfs_mount_create(mountpoint, backend_root);
    if (!m) return -ENOMEM;

    /* Initialize backend */
    void *backend_data = NULL;
    int ret = ops->init(backend_root, &backend_data);
    if (ret < 0) {
        vfs_mount_destroy(m);
        return ret;
    }

    /* Attach backend to mount */
    m->backend_ops = ops;
    m->backend_data = backend_data;

    return 0;
}
```

---

## STEP 3: Update VFS File Operations to Dispatch to Backends

### Goal
Route file operations through backend when mounted

### Key Concept: Calculate Relative Path
For a mount at `/mnt/data` with backend root `/tmp/backend`:
- VFS path: `/mnt/data/foo/bar.txt`
- Mount prefix: `/mnt/data`
- Relative path for backend: `foo/bar.txt`

### Implementation

**File: `src/core/vfs_core.c`**

Add helper function after find_best_mount:

```c
/* Calculate relative path within mount 
 * Returns newly allocated string or NULL on error 
 * Caller must free the returned string.
 */
static char *get_relpath_for_mount(const char *full_path, vfs_mount_entry_t *mount) {
    if (!full_path || !mount || !mount->mountpoint) return NULL;
    
    const char *mp = mount->mountpoint;
    size_t mp_len = strlen(mp);
    
    /* Root mount special case */
    if (strcmp(mp, "/") == 0) {
        /* Skip leading slash */
        const char *rel = (full_path[0] == '/' && full_path[1]) ? full_path + 1 : full_path;
        return strdup(rel[0] ? rel : ".");
    }
    
    /* Check if path starts with mountpoint */
    if (strncmp(full_path, mp, mp_len) != 0) {
        return NULL;
    }
    
    /* Extract relative part */
    const char *rel = full_path + mp_len;
    while (*rel == '/') rel++; /* skip separator */
    
    return strdup(rel[0] ? rel : ".");
}
```

Update `vfs_open` (find existing function and replace):

```c
int vfs_open(const char *path, int flags)
{
    if (!path) return -EINVAL;

    vfs_dentry_t *d = NULL;
    int ret = vfs_resolve_path(path, &d);
    if (ret < 0) return ret;

    if (!d || !d->inode) return -ENOENT;

    /* Check if directory */
    if (S_ISDIR(d->inode->mode)) return -EISDIR;

    /* Permission check */
    uid_t uid = getuid();
    gid_t gid = getgid();
    int mask = (flags & (O_RDWR | O_WRONLY)) ? W_OK : R_OK;
    if (check_inode_perm(d->inode, uid, gid, mask) < 0)
        return -EACCES;

    /* Check if mount has backend */
    vfs_mount_entry_t *mount = find_best_mount(path);
    if (mount && mount->backend_ops && mount->backend_ops->open) {
        /* Dispatch to backend */
        char *relpath = get_relpath_for_mount(path, mount);
        if (!relpath) return -EINVAL;
        
        void *backend_handle = NULL;
        ret = mount->backend_ops->open(mount->backend_data, relpath, flags, &backend_handle);
        free(relpath);
        
        if (ret < 0) return ret;
        
        /* Store backend handle in inode */
        d->inode->backend_handle = backend_handle;
    }

    /* Allocate file handle */
    int fh = fh_alloc(d, flags);
    return fh;
}
```

Update `vfs_read`:

```c
ssize_t vfs_read(int fh, void *buf, size_t count, off_t offset)
{
    if (!buf) return -EINVAL;

    vfs_fh_entry_t *e = fh_get(fh);
    if (!e || !e->dentry) return -EBADF;

    vfs_dentry_t *d = e->dentry;
    if (!d->inode) return -EBADF;

    /* Permission check */
    uid_t uid = getuid();
    gid_t gid = getgid();
    if (check_inode_perm(d->inode, uid, gid, R_OK) < 0)
        return -EACCES;

    /* Check if backend is available */
    if (d->inode->backend_handle) {
        /* Find mount for this dentry to get backend ops */
        char path_buf[PATH_MAX];
        /* Build path from dentry (simplified - assumes we can reconstruct) */
        snprintf(path_buf, sizeof(path_buf), "/%s", d->name);
        
        vfs_mount_entry_t *mount = find_best_mount(path_buf);
        if (mount && mount->backend_ops && mount->backend_ops->read) {
            return mount->backend_ops->read(mount->backend_data, 
                                            d->inode->backend_handle,
                                            buf, count, offset);
        }
    }

    /* Fallback: in-memory zero-filled read */
    off_t size = d->inode->size;
    if (offset >= size) return 0;

    size_t to_read = (offset + count > size) ? (size - offset) : count;
    memset(buf, 0, to_read);
    return to_read;
}
```

Update `vfs_write`:

```c
ssize_t vfs_write(int fh, const void *buf, size_t count, off_t offset)
{
    if (!buf) return -EINVAL;

    vfs_fh_entry_t *e = fh_get(fh);
    if (!e || !e->dentry) return -EBADF;

    vfs_dentry_t *d = e->dentry;
    if (!d->inode) return -EBADF;

    /* Permission check */
    uid_t uid = getuid();
    gid_t gid = getgid();
    if (check_inode_perm(d->inode, uid, gid, W_OK) < 0)
        return -EACCES;

    /* Check if backend is available */
    if (d->inode->backend_handle) {
        char path_buf[PATH_MAX];
        snprintf(path_buf, sizeof(path_buf), "/%s", d->name);
        
        vfs_mount_entry_t *mount = find_best_mount(path_buf);
        if (mount && mount->backend_ops && mount->backend_ops->write) {
            ssize_t written = mount->backend_ops->write(mount->backend_data,
                                                         d->inode->backend_handle,
                                                         buf, count, offset);
            if (written > 0) {
                /* Update size */
                off_t new_size = offset + written;
                if (new_size > d->inode->size)
                    d->inode->size = new_size;
            }
            return written;
        }
    }

    /* Fallback: in-memory write (just update size) */
    off_t new_size = offset + count;
    if (new_size > d->inode->size)
        d->inode->size = new_size;

    return count;
}
```

Update `vfs_stat`:

```c
int vfs_stat(const char *path, struct stat *st)
{
    if (!path || !st) return -EINVAL;

    /* Check if backend can provide stat */
    vfs_mount_entry_t *mount = find_best_mount(path);
    if (mount && mount->backend_ops && mount->backend_ops->stat) {
        char *relpath = get_relpath_for_mount(path, mount);
        if (relpath) {
            int ret = mount->backend_ops->stat(mount->backend_data, relpath, st);
            free(relpath);
            if (ret == 0) return 0;
            /* If backend fails, fall through to in-memory */
        }
    }

    /* Fallback: in-memory stat */
    vfs_dentry_t *d = NULL;
    int ret = vfs_resolve_path(path, &d);
    if (ret < 0) return ret;

    if (!d || !d->inode) return -ENOENT;

    memset(st, 0, sizeof(*st));
    st->st_ino = d->inode->ino;
    st->st_mode = d->inode->mode;
    st->st_uid = d->inode->uid;
    st->st_gid = d->inode->gid;
    st->st_size = d->inode->size;
    st->st_nlink = 1;

    return 0;
}
```

Update `vfs_readdir`:

```c
int vfs_readdir(const char *path, void *buf, void *filler)
{
    if (!path || !filler) return -EINVAL;

    /* Check if backend can provide readdir */
    vfs_mount_entry_t *mount = find_best_mount(path);
    if (mount && mount->backend_ops && mount->backend_ops->readdir) {
        char *relpath = get_relpath_for_mount(path, mount);
        if (relpath) {
            int ret = mount->backend_ops->readdir(mount->backend_data, relpath, buf, filler);
            free(relpath);
            if (ret == 0) return 0;
            /* If backend fails, fall through to in-memory */
        }
    }

    /* Fallback: in-memory readdir */
    vfs_dentry_t *d = NULL;
    int ret = vfs_resolve_path(path, &d);
    if (ret < 0) return ret;

    if (!d || !d->inode) return -ENOENT;
    if (!S_ISDIR(d->inode->mode)) return -ENOTDIR;

    typedef int (*fill_fn_t)(void *, const char *, const struct stat *, off_t);
    fill_fn_t fill = (fill_fn_t)filler;

    struct stat st;
    for (vfs_dentry_t *c = d->child; c; c = c->sibling) {
        if (c->inode) {
            memset(&st, 0, sizeof(st));
            st.st_ino = c->inode->ino;
            st.st_mode = c->inode->mode;
            fill(buf, c->name, &st, 0);
        }
    }

    return 0;
}
```

---

## STEP 4: Add Missing VFS API Functions for FUSE

### Goal
Implement functions FUSE expects but don't exist in VFS Core

**File: `src/core/vfs_core.h`**

Add these prototypes before the #endif:

```c
/* FUSE-compatible API extensions */
int vfs_destroy(void);                    /* alias for vfs_shutdown */
int vfs_getattr(const char *path, struct stat *stbuf);  /* alias for vfs_stat */
int vfs_create(const char *path, mode_t mode, struct fuse_file_info *fi);
int vfs_mkdir(const char *path, mode_t mode);
int vfs_mknod(const char *path, mode_t mode, dev_t rdev);
ssize_t vfs_readlink(const char *path, char *buf, size_t size);
int vfs_symlink(const char *target, const char *linkpath);
```

**File: `src/core/vfs_core.c`**

Add implementations at the end:

```c
/* -------------------------------------------------------------------------- */
/* FUSE-Compatible API Extensions */
/* -------------------------------------------------------------------------- */

int vfs_destroy(void) {
    return vfs_shutdown();
}

int vfs_getattr(const char *path, struct stat *stbuf) {
    return vfs_stat(path, stbuf);
}

int vfs_create(const char *path, mode_t mode, struct fuse_file_info *fi) {
    if (!path) return -EINVAL;
    (void)fi;  /* Unused for now */
    
    /* Check if backend available */
    vfs_mount_entry_t *mount = find_best_mount(path);
    if (mount && mount->backend_ops) {
        /* Backend should have create support - for now use open with O_CREAT */
        int flags = O_CREAT | O_EXCL | O_RDWR;
        return vfs_open(path, flags);
    }
    
    /* In-memory create */
    vfs_dentry_t *d = NULL;
    int ret = vfs_resolve_path(path, &d);
    if (ret < 0) return ret;
    
    if (d && d->inode) {
        /* Already exists */
        return -EEXIST;
    }
    
    /* Create new inode */
    uint64_t ino = g_next_ino++;
    vfs_inode_t *inode = vfs_inode_create(ino, mode | S_IFREG, getuid(), getgid(), 0);
    if (!inode) return -ENOMEM;
    
    /* Create dentry */
    char *name = strrchr(path, '/');
    name = name ? name + 1 : (char *)path;
    
    vfs_dentry_t *new_d = vfs_dentry_create(name, NULL, inode);
    vfs_inode_release(inode);
    
    if (!new_d) return -ENOMEM;
    
    return 0;
}

int vfs_mkdir(const char *path, mode_t mode) {
    if (!path) return -EINVAL;
    
    /* Similar to create but for directories */
    vfs_dentry_t *d = NULL;
    int ret = vfs_resolve_path(path, &d);
    if (ret < 0) return ret;
    
    if (d && d->inode) return -EEXIST;
    
    uint64_t ino = g_next_ino++;
    vfs_inode_t *inode = vfs_inode_create(ino, mode | S_IFDIR, getuid(), getgid(), 0);
    if (!inode) return -ENOMEM;
    
    char *name = strrchr(path, '/');
    name = name ? name + 1 : (char *)path;
    
    vfs_dentry_t *new_d = vfs_dentry_create(name, NULL, inode);
    vfs_inode_release(inode);
    
    return new_d ? 0 : -ENOMEM;
}

int vfs_mknod(const char *path, mode_t mode, dev_t rdev) {
    (void)rdev;  /* Not supported for now */
    return vfs_create(path, mode, NULL);
}

ssize_t vfs_readlink(const char *path, char *buf, size_t size) {
    if (!path || !buf || size == 0) return -EINVAL;
    
    /* Symlinks not implemented yet */
    (void)path;
    return -ENOSYS;
}

int vfs_symlink(const char *target, const char *linkpath) {
    if (!target || !linkpath) return -EINVAL;
    
    /* Symlinks not implemented yet */
    return -ENOSYS;
}
```

---

## STEP 5: Fix VFS/FUSE API Signature Mismatches

### Current Issues:
1. FUSE calls `vfs_open(path, fi)` but VFS has `vfs_open(path, flags)`
2. FUSE calls `vfs_read(path, ...)` but VFS has `vfs_read(fh, ...)`
3. FUSE calls `vfs_readdir(path, buf, filler, offset, fi)` but VFS has `vfs_readdir(path, buf, filler)`

### Solution: Create FUSE-specific wrappers

**File: `src/fuse/vfs_fuse.c`**

Update the VFS function declarations and add wrapper logic:

```c
/* Internal VFS Core API (with handle-based operations) */
extern int vfs_open_internal(const char *path, int flags);
extern int vfs_close(int fh);
extern ssize_t vfs_read_internal(int fh, void *buf, size_t count, off_t offset);
extern ssize_t vfs_write_internal(int fh, const void *buf, size_t count, off_t offset);

/* Wrapper: vfs_open for FUSE (stores handle in fi->fh) */
int vfs_open(const char *path, struct fuse_file_info *fi) {
    if (!path || !fi) return -EINVAL;
    
    int fh = vfs_open_internal(path, fi->flags);
    if (fh < 0) return fh;
    
    fi->fh = fh;  /* Store file handle in FUSE structure */
    return 0;
}

/* Wrapper: vfs_read for FUSE (extracts path from context - simplified) */
ssize_t vfs_read(const char *path, char *buf, size_t size, off_t offset) {
    /* For now, open file each time - suboptimal but functional */
    int fh = vfs_open_internal(path, O_RDONLY);
    if (fh < 0) return fh;
    
    ssize_t ret = vfs_read_internal(fh, buf, size, offset);
    vfs_close(fh);
    return ret;
}

/* Wrapper: vfs_write for FUSE */
ssize_t vfs_write(const char *path, const char *buf, size_t size, off_t offset) {
    int fh = vfs_open_internal(path, O_WRONLY);
    if (fh < 0) return fh;
    
    ssize_t ret = vfs_write_internal(fh, buf, size, offset);
    vfs_close(fh);
    return ret;
}

/* Wrapper: vfs_readdir for FUSE */
int vfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                off_t offset, struct fuse_file_info *fi) {
    (void)offset;
    (void)fi;
    
    extern int vfs_readdir_internal(const char *path, void *buf, void *filler);
    return vfs_readdir_internal(path, buf, (void *)filler);
}
```

**File: `src/core/vfs_core.c`**

Rename existing functions to _internal versions:

```c
/* Rename vfs_open to vfs_open_internal */
int vfs_open_internal(const char *path, int flags) {
    /* existing implementation */
}

/* Rename vfs_read to vfs_read_internal */
ssize_t vfs_read_internal(int fh, void *buf, size_t count, off_t offset) {
    /* existing implementation */
}

/* Rename vfs_write to vfs_write_internal */
ssize_t vfs_write_internal(int fh, const void *buf, size_t count, off_t offset) {
    /* existing implementation */
}

/* Rename vfs_readdir to vfs_readdir_internal */
int vfs_readdir_internal(const char *path, void *buf, void *filler) {
    /* existing implementation */
}
```

**Alternative Simpler Approach:**
Keep VFS Core functions as-is, and in vfs_fuse.c, explicitly map between handle-based and path-based operations using a thread-local or per-fi storage.

---

## STEP 6: Initialize Backend at Startup

### Goal
Register POSIX backend and create test mount

**File: `src/core/vfs_core.c`**

Update `vfs_init()`:

```c
int vfs_init(void)
{
    pthread_mutex_lock(&g_vfs_lock);

    if (g_vfs_inited) {
        pthread_mutex_unlock(&g_vfs_lock);
        return 0;
    }

    /* Initialize file handle table */
    fh_table_init_once();

    /* Register POSIX backend */
    extern const vfs_backend_ops_t *get_posix_backend_ops(void);
    const vfs_backend_ops_t *posix_ops = get_posix_backend_ops();
    if (posix_ops) {
        int ret = vfs_register_backend(posix_ops);
        if (ret < 0 && ret != -EEXIST) {
            fprintf(stderr, "vfs_init: failed to register posix backend: %d\n", ret);
        }
    }

    /* Create default root mount (in-memory) */
    vfs_mount_entry_t *root_m = vfs_mount_create("/", "/");
    if (!root_m) {
        pthread_mutex_unlock(&g_vfs_lock);
        return -ENOMEM;
    }

    /* Create sample directory tree */
    uint64_t ino_root = g_next_ino++;
    vfs_inode_t *ino_r = vfs_inode_create(ino_root, S_IFDIR | 0755, 0, 0, 0);
    vfs_dentry_t *droot = vfs_dentry_create("/", NULL, ino_r);
    vfs_inode_release(ino_r);
    root_m->root_dentry = droot;

    uint64_t ino1 = g_next_ino++;
    vfs_inode_t *ino_d1 = vfs_inode_create(ino1, S_IFDIR | 0755, 0, 0, 0);
    vfs_dentry_t *d1 = vfs_dentry_create("dir1", droot, ino_d1);
    vfs_inode_release(ino_d1);
    vfs_dentry_add_child(droot, d1);

    g_vfs_inited = 1;
    pthread_mutex_unlock(&g_vfs_lock);

    fprintf(stderr, "[vfs_init] VFS initialized with POSIX backend registered\n");
    return 0;
}
```

---

## STEP 7: Integration Testing

### Create Integration Test

**File: `tests/test_integration.c`**

```c
#include "../src/core/vfs_core.h"
#include "../src/backends/backend_posix.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

int main(void) {
    printf("=== VFS Integration Test ===\n\n");
    
    /* Initialize VFS */
    printf("1. Initializing VFS...\n");
    int ret = vfs_init();
    if (ret != 0) {
        fprintf(stderr, "FAIL: vfs_init returned %d\n", ret);
        return 1;
    }
    printf("   ✓ VFS initialized\n\n");
    
    /* Create test directory */
    printf("2. Creating test backend directory...\n");
    system("mkdir -p /tmp/vfs_test_backend");
    printf("   ✓ Created /tmp/vfs_test_backend\n\n");
    
    /* Mount POSIX backend */
    printf("3. Mounting POSIX backend at /backend...\n");
    ret = vfs_mount_backend("/backend", "/tmp/vfs_test_backend", "posix");
    if (ret != 0) {
        fprintf(stderr, "FAIL: vfs_mount_backend returned %d\n", ret);
        vfs_shutdown();
        return 1;
    }
    printf("   ✓ Backend mounted\n\n");
    
    /* Test file creation through backend */
    printf("4. Creating test file /backend/test.txt...\n");
    int fh = vfs_open("/backend/test.txt", O_CREAT | O_RDWR);
    if (fh < 0) {
        fprintf(stderr, "FAIL: vfs_open returned %d\n", fh);
        vfs_shutdown();
        return 1;
    }
    printf("   ✓ File created (handle %d)\n\n", fh);
    
    /* Write data */
    printf("5. Writing data to file...\n");
    const char *data = "Hello VFS Integration!\n";
    ssize_t written = vfs_write(fh, data, strlen(data), 0);
    if (written < 0) {
        fprintf(stderr, "FAIL: vfs_write returned %ld\n", written);
        vfs_close(fh);
        vfs_shutdown();
        return 1;
    }
    printf("   ✓ Wrote %ld bytes\n\n", written);
    
    /* Read data back */
    printf("6. Reading data from file...\n");
    char buf[256] = {0};
    ssize_t nread = vfs_read(fh, buf, sizeof(buf) - 1, 0);
    if (nread < 0) {
        fprintf(stderr, "FAIL: vfs_read returned %ld\n", nread);
        vfs_close(fh);
        vfs_shutdown();
        return 1;
    }
    buf[nread] = '\0';
    printf("   ✓ Read %ld bytes: \"%s\"\n\n", nread, buf);
    
    /* Verify data */
    if (strcmp(buf, data) != 0) {
        fprintf(stderr, "FAIL: Data mismatch!\n");
        fprintf(stderr, "  Expected: \"%s\"\n", data);
        fprintf(stderr, "  Got:      \"%s\"\n", buf);
        vfs_close(fh);
        vfs_shutdown();
        return 1;
    }
    printf("   ✓ Data verified\n\n");
    
    /* Close file */
    printf("7. Closing file...\n");
    ret = vfs_close(fh);
    if (ret < 0) {
        fprintf(stderr, "FAIL: vfs_close returned %d\n", ret);
        vfs_shutdown();
        return 1;
    }
    printf("   ✓ File closed\n\n");
    
    /* Test stat */
    printf("8. Testing stat on /backend/test.txt...\n");
    struct stat st;
    ret = vfs_stat("/backend/test.txt", &st);
    if (ret != 0) {
        fprintf(stderr, "FAIL: vfs_stat returned %d\n", ret);
        vfs_shutdown();
        return 1;
    }
    printf("   ✓ Stat: size=%ld mode=0%o\n\n", (long)st.st_size, st.st_mode & 0777);
    
    /* Cleanup */
    printf("9. Shutting down VFS...\n");
    ret = vfs_shutdown();
    if (ret != 0) {
        fprintf(stderr, "FAIL: vfs_shutdown returned %d\n", ret);
        return 1;
    }
    printf("   ✓ VFS shutdown\n\n");
    
    printf("=== ALL TESTS PASSED ===\n");
    return 0;
}
```

**Update Makefile:**

```makefile
# Add to test targets
.PHONY: test_integration
test_integration: $(CORE_SRC:.c=.o) $(BACKEND_SRC:.c=.o)
	$(CC) $(CFLAGS) -o $@ tests/test_integration.c $^ $(LIBS)
	./test_integration

# Update test target
.PHONY: test
test: test_core test_lookup test_file_ops test_integration
```

---

## Summary of Changes

### Files to Modify:
1. **src/backends/backend_posix.c** - Add adapter functions and backend ops structure
2. **src/backends/backend_posix.h** - Add get_posix_backend_ops() declaration
3. **src/core/vfs_core.c** - Add registry, update mount/file operations
4. **src/core/vfs_core.h** - Add vfs_register_backend, FUSE API functions
5. **src/fuse/vfs_fuse.c** - Create wrappers for signature mismatches
6. **tests/test_integration.c** - New integration test file
7. **Makefile** - Add integration test target

### Build Order:
```bash
# Step 1-2: Backend ops + Registry
make clean
make

# Step 3-4: File ops dispatch + FUSE API
make clean
make

# Step 5-6: Signature fixes + Init
make clean
make

# Step 7: Integration test
make test_integration
```

---

## Next Steps

Ready to implement? We can proceed:
- **One step at a time** - I'll implement each step and test before moving on
- **All at once** - Implement all 7 steps in parallel
- **Custom order** - You choose which steps to tackle first

Which approach would you prefer?
