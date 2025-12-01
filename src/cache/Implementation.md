vfs-project/
├─ src/
│  ├─ core/
│  │   ├─ vfs_core.h
│  │   ├─ vfs_core.c
│  │   ├─ vfs_cache.h        <── NEW (public cache API for Core)
│  │   └─ vfs_cache.c        <── NEW (Working Set Model cache implementation)
│  ├─ backends/
│  │   ├─ backend_posix.h
│  │   └─ backend_posix.c
│  ├─ fuse/
│  │   ├─ vfs_fuse.h
│  │   └─ vfs_fuse.c
│  ├─ utils/
│  │   ├─ lru_list.h         <── helper (optional) 
│  │   └─ lru_list.c
├─ tests/
│  ├─ test_cache_basic.c     <── NEW
│  ├─ test_cache_wsm.c       <── NEW (Working Set Model test)
│  └─ ...
└─ Makefile


### **2. vfs_core.c**
You **modify vfs_read(), vfs_write(), vfs_stat()** to include calls to the cache.

### **3. backend layer (backend_posix.c)**
No modifications except:
- backend functions must be callable from cache on cache miss
- backend_read / backend_write used inside cache

---

# 🚀 **Core Idea of Working Set Model (WSM) for VFS**
The Working Set Model maintains a "window" of recently accessed pages **per file**.

Your implementation:

- Track pages using `(backend_id, path_hash, page_index)`
- Maintain WS(t) = set of pages used in last Δ time interval (configurable)
- If pages exceed WS size → evict least recently used (LRU inside window)
- TTL expiration removes old pages outside window

This improves locality and prevents thrashing.

---

# ✅ **What Files You Must Create / Modify**

---

## **🔹 1. Create `src/core/vfs_cache.h`**
Contains the **cache API**, used by the VFS Core:

### Recommended API:
```c
#ifndef VFS_CACHE_H
#define VFS_CACHE_H

#include <stddef.h>
#include <stdint.h>

int cache_init(size_t max_pages, size_t working_set_window_ms);
int cache_shutdown();

ssize_t cache_read(
    int backend_id,
    const char *relpath,
    void *buf,
    size_t size,
    off_t offset,
    int *was_cache_hit
);

int cache_write_through(
    int backend_id,
    const char *relpath,
    const void *buf,
    size_t size,
    off_t offset
);

int cache_invalidate_file(int backend_id, const char *relpath);
int cache_invalidate_all();

#endif
```

---

## **🔹 2. Create `src/core/vfs_cache.c`**
**Implements the Working Set Model:**

Must include:

### Data structures

```
cache_bucket (hash)
 └─ WSList = working set tracking (per Path)
       ├─ timestamp
       ├─ page index
       └─ data (4KB)
```

### Required Internal Functions
```c
static cache_page_t *lookup_page(...);
static void add_page_to_ws(...);
static void evict_pages_outside_working_set(...);
static void evict_if_over_capacity();
```

### Public functions required by VFS Core
(you integrate these)

- `cache_read()`
- `cache_write_through()`
- `cache_invalidate_file()`
- `cache_invalidate_all()`

---

## **🔹 3. Modify `src/core/vfs_core.c`**

You must integrate the cache here.

### Integrate into vfs_read()
```c
ssize_t vfs_read(int fh, void *buf, size_t size, off_t offset)
{
    // 1. Query cache first
    int hit = 0;
    ssize_t r = cache_read(backend_id, relpath, buf, size, offset, &hit);

    if (r >= 0 && hit) return r;

    // 2. Cache miss → backend read
    r = backend_read(backend_id, handle, buf, size, offset);

    if (r > 0) {
        // 3. Put into cache using Working Set Model
        cache_write_through(backend_id, relpath, buf, r, offset);
    }

    return r;
}
```

### Integrate into vfs_write()
```c
ssize_t vfs_write(int fh, const void *buf, size_t size, off_t offset)
{
    // Write-through
    ssize_t written = backend_write(backend_id, handle, buf, size, offset);

    if (written > 0) {
        cache_write_through(backend_id, relpath, buf, written, offset);
    }

    return written;
}
```

### Integrate into vfs_unlink(), vfs_rename()
Invalidate cache for those files:
```c
cache_invalidate_file(backend_id, relpath);
```

---
