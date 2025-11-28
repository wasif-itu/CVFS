/* Ensure correct FUSE API visible to headers */
#ifndef FUSE_USE_VERSION
#define FUSE_USE_VERSION 30
#endif

#define _GNU_SOURCE
#include "backend_posix.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <limits.h>
#include <fcntl.h>
#include <dirent.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <unistd.h>
#include <stdint.h>

/* Use fuse's filler type if available */
#ifdef __has_include
# if __has_include(<fuse3/fuse.h>)
#  include <fuse3/fuse.h>
#  define USE_FUSE_FILLER 1
# endif
#endif

#ifndef USE_FUSE_FILLER
/* fallback: ensure compatible signature */
typedef int (*fuse_fill_dir_t)(void *buf, const char *name, const struct stat *st, off_t off);
#endif

/* ---------- rest of implementation ---------- */
/* (the existing implementation follows unchanged) */

#define PATH_BUFSZ PATH_MAX
#define INITIAL_HANDLE_CAP 16

typedef struct backend_handle {
    int fd;
    int in_use;
} backend_handle_t;

typedef struct posix_backend {
    int id;
    char *rootpath;                  /* absolute path to backend root */
    pthread_mutex_t lock;            /* protects handles/table */
    backend_handle_t *handles;       /* dynamic array indexed by (handle-1) */
    size_t handle_cap;
} posix_backend_t;

/* Simple global registry for backends */
#define MAX_BACKENDS 32
static posix_backend_t *backends[MAX_BACKENDS];
static pthread_mutex_t backends_lock = PTHREAD_MUTEX_INITIALIZER;

/* Helpers */

static posix_backend_t *get_backend(int backend_id) {
    if (backend_id <= 0 || backend_id > MAX_BACKENDS) return NULL;
    return backends[backend_id - 1];
}

static int allocate_backend_slot(posix_backend_t *b) {
    pthread_mutex_lock(&backends_lock);
    for (int i = 0; i < MAX_BACKENDS; ++i) {
        if (backends[i] == NULL) {
            backends[i] = b;
            pthread_mutex_unlock(&backends_lock);
            return i + 1;
        }
    }
    pthread_mutex_unlock(&backends_lock);
    errno = ENOMEM;
    return -1;
}

static void free_backend_slot(int backend_id) {
    if (backend_id <= 0 || backend_id > MAX_BACKENDS) return;
    pthread_mutex_lock(&backends_lock);
    backends[backend_id - 1] = NULL;
    pthread_mutex_unlock(&backends_lock);
}

/* join rootpath and relpath safely into out (size out_sz). relpath must be relative */
static int join_backend_path(const char *root, const char *relpath, char *out, size_t out_sz) {
    if (!root || !relpath || !out) { errno = EINVAL; return -1; }
    if (relpath[0] == '/') {
        /* disallow absolute relpath for safety */
        errno = EINVAL;
        return -1;
    }
    int n = snprintf(out, out_sz, "%s/%s", root, relpath);
    if (n < 0 || (size_t)n >= out_sz) { errno = ENAMETOOLONG; return -1; }
    return 0;
}

/* Ensure backend handle table has capacity */
static int ensure_handle_capacity(posix_backend_t *b, size_t min_cap) {
    if (b->handle_cap >= min_cap) return 0;
    size_t new_cap = b->handle_cap ? b->handle_cap * 2 : INITIAL_HANDLE_CAP;
    while (new_cap < min_cap) new_cap *= 2;
    backend_handle_t *new_table = realloc(b->handles, new_cap * sizeof(backend_handle_t));
    if (!new_table) return -1;
    /* initialize new entries */
    for (size_t i = b->handle_cap; i < new_cap; ++i) {
        new_table[i].in_use = 0;
        new_table[i].fd = -1;
    }
    b->handles = new_table;
    b->handle_cap = new_cap;
    return 0;
}

/* Create a new handle entry for fd, return handle (>0) or -1 */
static int create_handle(posix_backend_t *b, int fd) {
    if (!b) { errno = EINVAL; return -1; }
    pthread_mutex_lock(&b->lock);
    /* find free slot */
    for (size_t i = 0; i < b->handle_cap; ++i) {
        if (!b->handles[i].in_use) {
            b->handles[i].in_use = 1;
            b->handles[i].fd = fd;
            int handle = (int)(i + 1);
            pthread_mutex_unlock(&b->lock);
            return handle;
        }
    }
    /* need more capacity */
    size_t need = b->handle_cap ? b->handle_cap * 2 : INITIAL_HANDLE_CAP;
    if (ensure_handle_capacity(b, need) != 0) {
        pthread_mutex_unlock(&b->lock);
        errno = ENOMEM;
        return -1;
    }
    /* after resize, allocate first new slot */
    for (size_t i = 0; i < b->handle_cap; ++i) {
        if (!b->handles[i].in_use) {
            b->handles[i].in_use = 1;
            b->handles[i].fd = fd;
            int handle = (int)(i + 1);
            pthread_mutex_unlock(&b->lock);
            return handle;
        }
    }
    pthread_mutex_unlock(&b->lock);
    errno = ENOMEM;
    return -1;
}

/* Lookup handle -> fd, return fd or -1 and set errno */
static int lookup_fd(posix_backend_t *b, int handle) {
    if (!b || handle <= 0) { errno = EINVAL; return -1; }
    size_t idx = (size_t)(handle - 1);
    pthread_mutex_lock(&b->lock);
    if (idx >= b->handle_cap || !b->handles[idx].in_use) {
        pthread_mutex_unlock(&b->lock);
        errno = EBADF;
        return -1;
    }
    int fd = b->handles[idx].fd;
    pthread_mutex_unlock(&b->lock);
    return fd;
}

/* free handle */
static int free_handle(posix_backend_t *b, int handle) {
    if (!b || handle <= 0) { errno = EINVAL; return -1; }
    size_t idx = (size_t)(handle - 1);
    pthread_mutex_lock(&b->lock);
    if (idx >= b->handle_cap || !b->handles[idx].in_use) {
        pthread_mutex_unlock(&b->lock);
        errno = EBADF;
        return -1;
    }
    b->handles[idx].in_use = 0;
    b->handles[idx].fd = -1;
    pthread_mutex_unlock(&b->lock);
    return 0;
}

/* Public API implementations */

int posix_backend_init(const char *rootpath) {
    if (!rootpath) { errno = EINVAL; return -1; }

    posix_backend_t *b = calloc(1, sizeof(*b));
    if (!b) { errno = ENOMEM; return -1; }

    b->rootpath = strdup(rootpath);
    if (!b->rootpath) {
        free(b);
        errno = ENOMEM;
        return -1;
    }
    if (pthread_mutex_init(&b->lock, NULL) != 0) {
        free(b->rootpath);
        free(b);
        errno = ENOMEM;
        return -1;
    }
    b->handles = NULL;
    b->handle_cap = 0;

    int id = allocate_backend_slot(b);
    if (id < 0) {
        pthread_mutex_destroy(&b->lock);
        free(b->rootpath);
        free(b);
        return -1;
    }
    b->id = id;
    return id;
}

int posix_backend_shutdown(int backend_id) {
    posix_backend_t *b = get_backend(backend_id);
    if (!b) { errno = EINVAL; return -1; }

    /* close any open fds */
    pthread_mutex_lock(&b->lock);
    for (size_t i = 0; i < b->handle_cap; ++i) {
        if (b->handles && b->handles[i].in_use) {
            close(b->handles[i].fd);
            b->handles[i].in_use = 0;
            b->handles[i].fd = -1;
        }
    }
    pthread_mutex_unlock(&b->lock);

    /* free resources */
    if (b->handles) free(b->handles);
    free(b->rootpath);
    pthread_mutex_destroy(&b->lock);

    free_backend_slot(backend_id);
    free(b);
    return 0;
}

int posix_open(int backend_id, const char *relpath, int flags, mode_t mode) {
    posix_backend_t *b = get_backend(backend_id);
    if (!b) { errno = EINVAL; return -1; }

    char full[PATH_BUFSZ];
    if (join_backend_path(b->rootpath, relpath, full, sizeof(full)) != 0) {
        return -1;
    }

    int fd = open(full, flags, mode);
    if (fd < 0) return -1;

    int handle = create_handle(b, fd);
    if (handle < 0) {
        /* failed to create logical handle; close FD to avoid leak */
        close(fd);
        return -1;
    }
    return handle;
}

int posix_close(int backend_id, int handle) {
    posix_backend_t *b = get_backend(backend_id);
    if (!b) { errno = EINVAL; return -1; }

    int fd = lookup_fd(b, handle);
    if (fd < 0) return -1;

    if (close(fd) != 0) return -1;
    return free_handle(b, handle);
}

ssize_t posix_read(int backend_id, int handle, void *buf, size_t count, off_t offset) {
    posix_backend_t *b = get_backend(backend_id);
    if (!b) { errno = EINVAL; return -1; }

    int fd = lookup_fd(b, handle);
    if (fd < 0) return -1;

    ssize_t r = pread(fd, buf, count, offset);
    return r;
}

ssize_t posix_write(int backend_id, int handle, const void *buf, size_t count, off_t offset) {
    posix_backend_t *b = get_backend(backend_id);
    if (!b) { errno = EINVAL; return -1; }

    int fd = lookup_fd(b, handle);
    if (fd < 0) return -1;

    ssize_t w = pwrite(fd, buf, count, offset);
    return w;
}

int posix_stat(int backend_id, const char *relpath, struct stat *st) {
    posix_backend_t *b = get_backend(backend_id);
    if (!b || !st) { errno = EINVAL; return -1; }

    char full[PATH_BUFSZ];
    if (join_backend_path(b->rootpath, relpath, full, sizeof(full)) != 0) {
        return -1;
    }

    if (stat(full, st) != 0) return -1;
    return 0;
}

int posix_readdir(int backend_id, const char *relpath, void *buf, vfs_fill_dir_t filler, off_t offset) {
    posix_backend_t *b = get_backend(backend_id);
    if (!b || !filler) { errno = EINVAL; return -1; }

    char full[PATH_BUFSZ];
    if (join_backend_path(b->rootpath, relpath, full, sizeof(full)) != 0) {
        return -1;
    }

    DIR *d = opendir(full);
    if (!d) return -1;

    struct dirent *de;
    struct stat stbuf;
    while ((de = readdir(d)) != NULL) {
        /* Skip . and ..? Let filler decide; common to include them */
        char childpath[PATH_BUFSZ];
        int rc = snprintf(childpath, sizeof(childpath), "%s/%s", full, de->d_name);
        if (rc < 0 || (size_t)rc >= sizeof(childpath)) {
            continue;
        }
        if (stat(childpath, &stbuf) != 0) {
            memset(&stbuf, 0, sizeof(stbuf));
        }
        /* call the filler - return value ignored here (FUSE uses non-zero to stop) */
        filler(buf, de->d_name, &stbuf, 0);
    }

    closedir(d);
    return 0;
}

/* Create file: open with O_CREAT|O_EXCL and return a handle */
int posix_create(int backend_id, const char *relpath, mode_t mode) {
    posix_backend_t *b = get_backend(backend_id);
    if (!b || !relpath) { errno = EINVAL; return -1; }

    char full[PATH_BUFSZ];
    if (join_backend_path(b->rootpath, relpath, full, sizeof(full)) != 0) {
        return -1;
    }

    int fd = open(full, O_CREAT | O_EXCL | O_RDWR, mode);
    if (fd < 0) return -1;

    int handle = create_handle(b, fd);
    if (handle < 0) {
        close(fd);
        return -1;
    }
    return handle;
}

/* Unlink a file or rmdir if directory (we use unlink for files, rmdir for dirs) */
int posix_unlink(int backend_id, const char *relpath) {
    posix_backend_t *b = get_backend(backend_id);
    if (!b || !relpath) { errno = EINVAL; return -1; }

    char full[PATH_BUFSZ];
    if (join_backend_path(b->rootpath, relpath, full, sizeof(full)) != 0) {
        return -1;
    }

    /* Try unlink first */
    if (unlink(full) == 0) return 0;

    /* If unlink failed with EISDIR, try rmdir */
    if (errno == EISDIR) {
        if (rmdir(full) == 0) return 0;
    }
    return -1;
}

/* Rename within backend */
int posix_rename(int backend_id, const char *old_relpath, const char *new_relpath) {
    posix_backend_t *b = get_backend(backend_id);
    if (!b || !old_relpath || !new_relpath) { errno = EINVAL; return -1; }

    char full_old[PATH_BUFSZ];
    char full_new[PATH_BUFSZ];
    if (join_backend_path(b->rootpath, old_relpath, full_old, sizeof(full_old)) != 0) {
        return -1;
    }
    if (join_backend_path(b->rootpath, new_relpath, full_new, sizeof(full_new)) != 0) {
        return -1;
    }

    if (rename(full_old, full_new) != 0) return -1;
    return 0;
}

/* Make directory */
int posix_mkdir(int backend_id, const char *relpath, mode_t mode) {
    posix_backend_t *b = get_backend(backend_id);
    if (!b || !relpath) { errno = EINVAL; return -1; }

    char full[PATH_BUFSZ];
    if (join_backend_path(b->rootpath, relpath, full, sizeof(full)) != 0) {
        return -1;
    }

    if (mkdir(full, mode) != 0) return -1;
    return 0;
}

/* ========================================================================
 * VFS Backend Ops Adapters
 * ======================================================================== */

#include "../core/vfs_core.h"
#include <stdint.h>

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
