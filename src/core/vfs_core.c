/*
 * Clean minimal VFS core:
 * - mount table
 * - dentry + inode management
 * - path normalization
 * - path resolution
 */

#include "vfs_core.h"
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <fcntl.h>

/* -------------------------------------------------------------------------- */
/* GLOBAL STATE */
/* -------------------------------------------------------------------------- */
static pthread_mutex_t g_vfs_lock = PTHREAD_MUTEX_INITIALIZER;

vfs_mount_entry_t *mount_table_head = NULL;
static uint64_t g_next_ino = 1000;
static int g_vfs_inited = 0;

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

/* -------------------------------------------------------------------------- */
/* File handle table (simple)                                                  */
/* -------------------------------------------------------------------------- */
#define VFS_MAX_FH 1024
typedef struct vfs_fh_entry {
    int in_use;
    vfs_dentry_t *dentry;
    int flags;
    off_t pos;
    pthread_mutex_t lock;
} vfs_fh_entry_t;

static vfs_fh_entry_t g_fh_table[VFS_MAX_FH];

static void fh_table_init_once(void)
{
    static int inited = 0;
    if (inited) return;
    for (int i = 0; i < VFS_MAX_FH; i++) {
        g_fh_table[i].in_use = 0;
        g_fh_table[i].dentry = NULL;
        g_fh_table[i].flags = 0;
        g_fh_table[i].pos = 0;
        pthread_mutex_init(&g_fh_table[i].lock, NULL);
    }
    inited = 1;
}

static int fh_alloc(vfs_dentry_t *d, int flags)
{
    fh_table_init_once();
    for (int i = 0; i < VFS_MAX_FH; i++) {
        if (!g_fh_table[i].in_use) {
            pthread_mutex_lock(&g_fh_table[i].lock);
            if (!g_fh_table[i].in_use) {
                g_fh_table[i].in_use = 1;
                g_fh_table[i].dentry = d;
                g_fh_table[i].flags = flags;
                g_fh_table[i].pos = 0;
                pthread_mutex_unlock(&g_fh_table[i].lock);
                return i + 1; /* handle id */
            }
            pthread_mutex_unlock(&g_fh_table[i].lock);
        }
    }
    return -EMFILE;
}

static vfs_fh_entry_t *fh_get(int fh)
{
    if (fh <= 0) return NULL;
    int idx = fh - 1;
    if (idx < 0 || idx >= VFS_MAX_FH) return NULL;
    if (!g_fh_table[idx].in_use) return NULL;
    return &g_fh_table[idx];
}

static void fh_free(int fh)
{
    vfs_fh_entry_t *e = fh_get(fh);
    if (!e) return;
    pthread_mutex_lock(&e->lock);
    vfs_dentry_t *d = e->dentry;
    e->in_use = 0;
    e->dentry = NULL;
    e->flags = 0;
    e->pos = 0;
    pthread_mutex_unlock(&e->lock);
    
    /* Release dentry reference held by file handle */
    if (d) {
        vfs_dentry_release(d);
    }
}

/* -------------------------------------------------------------------------- */
/* PATH NORMALIZATION */
/* -------------------------------------------------------------------------- */

static char *normalize_path_alloc(const char *path)
{
    if (!path || path[0] != '/')
        return NULL;

    char *buf = strdup(path);
    if (!buf)
        return NULL;

    size_t len = strlen(buf);
    char **stack = calloc(len + 1, sizeof(char *));
    if (!stack) {
        free(buf);
        return NULL;
    }

    size_t top = 0;
    char *p = buf;

    while (*p) {
        while (*p == '/')
            p++;
        if (!*p)
            break;

        char *seg = p;

        while (*p && *p != '/')
            p++;

        char saved = *p;
        *p = '\0';

        if (strcmp(seg, ".") == 0) {
            /* skip */
        } else if (strcmp(seg, "..") == 0) {
            if (top > 0) top--;
        } else {
            stack[top++] = seg;
        }

        *p = saved;
    }

    /* If root */
    if (top == 0) {
        free(stack);
        free(buf);
        return strdup("/");
    }

    /* Build output string */
    size_t out_len = 1; /* leading '/' */
    for (size_t i = 0; i < top; i++)
        out_len += strlen(stack[i]) + 1;

    char *res = calloc(1, out_len + 1);
    if (!res) {
        free(stack);
        free(buf);
        return NULL;
    }

    char *q = res;
    *q++ = '/';

    for (size_t i = 0; i < top; i++) {
        size_t n = strlen(stack[i]);
        memcpy(q, stack[i], n);
        q += n;
        if (i + 1 < top)
            *q++ = '/';
    }
    *q = '\0';

    free(stack);
    free(buf);
    return res;
}

/* -------------------------------------------------------------------------- */
/* INODE HELPERS */
/* -------------------------------------------------------------------------- */

vfs_inode_t *vfs_inode_create(uint64_t ino, mode_t mode,
                              uid_t uid, gid_t gid, off_t size)
{
    vfs_inode_t *n = calloc(1, sizeof(*n));
    if (!n)
        return NULL;

    n->ino = ino;
    n->mode = mode;
    n->uid = uid;
    n->gid = gid;
    n->size = size;
    n->refcount = 1;

    pthread_mutex_init(&n->lock, NULL);
    return n;
}

void vfs_inode_acquire(vfs_inode_t *ino)
{
    if (!ino) return;
    pthread_mutex_lock(&ino->lock);
    ino->refcount++;
    pthread_mutex_unlock(&ino->lock);
}

void vfs_inode_release(vfs_inode_t *ino)
{
    if (!ino) return;

    int free_now = 0;
    pthread_mutex_lock(&ino->lock);
    if (--ino->refcount == 0)
        free_now = 1;
    pthread_mutex_unlock(&ino->lock);

    if (free_now) {
        pthread_mutex_destroy(&ino->lock);
        free(ino);
    }
}

/* -------------------------------------------------------------------------- */
/* DENTRY HELPERS */
/* -------------------------------------------------------------------------- */

vfs_dentry_t *vfs_dentry_create(const char *name,
                                vfs_dentry_t *parent,
                                vfs_inode_t *inode)
{
    vfs_dentry_t *d = calloc(1, sizeof(*d));
    if (!d)
        return NULL;

    d->name = strdup(name ? name : "");
    d->parent = parent;
    d->inode  = inode;

    if (inode)
        vfs_inode_acquire(inode);

    pthread_mutex_init(&d->lock, NULL);
    return d;
}

void vfs_dentry_add_child(vfs_dentry_t *parent, vfs_dentry_t *child)
{
    if (!parent || !child)
        return;

    pthread_mutex_lock(&parent->lock);
    child->sibling = parent->child;
    parent->child = child;
    child->parent = parent;
    pthread_mutex_unlock(&parent->lock);
}

void vfs_dentry_remove_child(vfs_dentry_t *parent, vfs_dentry_t *child)
{
    if (!parent || !child)
        return;

    pthread_mutex_lock(&parent->lock);
    vfs_dentry_t **cur = &parent->child;
    while (*cur) {
        if (*cur == child) {
            *cur = child->sibling;
            child->sibling = NULL;
            child->parent = NULL;
            break;
        }
        cur = &(*cur)->sibling;
    }
    pthread_mutex_unlock(&parent->lock);
}

static void destroy_dentry_only(vfs_dentry_t *d)
{
    pthread_mutex_destroy(&d->lock);
    if (d->inode)
        vfs_inode_release(d->inode);
    free(d->name);
    free(d);
}

void vfs_dentry_destroy(vfs_dentry_t *dentry)
{
    if (!dentry)
        return;

    /* Detach from parent if present */
    if (dentry->parent) {
        vfs_dentry_remove_child(dentry->parent, dentry);
    }

    /* Destroy subtree rooted at this dentry */
    vfs_dentry_destroy_tree(dentry);
}

void vfs_dentry_release(vfs_dentry_t *dentry)
{
    if (!dentry) return;
    
    /* For orphaned dentries (no parent), free them directly */
    if (!dentry->parent) {
        destroy_dentry_only(dentry);
        return;
    }
    
    /* For dentries with parents, they'll be cleaned up with the tree */
    /* This is a no-op for now - could implement ref counting later */
}

void vfs_dentry_destroy_tree(vfs_dentry_t *root)
{
    if (!root) return;

    vfs_dentry_t *c = root->child;
    while (c) {
        vfs_dentry_t *next = c->sibling;
        vfs_dentry_destroy_tree(c);
        c = next;
    }
    destroy_dentry_only(root);
}

/* -------------------------------------------------------------------------- */
/* MOUNTING */
/* -------------------------------------------------------------------------- */

vfs_mount_entry_t *vfs_mount_create(const char *mountpoint,
                                const char *backend_root)
{
    vfs_mount_entry_t *m = calloc(1, sizeof(*m));
    if (!m)
        return NULL;

    m->mountpoint = strdup(mountpoint);
    m->backend_root = strdup(backend_root);
    m->backend_ops = NULL;  /* No backend by default */
    m->backend_data = NULL;

    /* Create synthetic root inode + dentry */
    uint64_t ino = g_next_ino++;
    vfs_inode_t *ri = vfs_inode_create(ino, S_IFDIR | 0755, 0, 0, 0);
    m->root_dentry = vfs_dentry_create("/", NULL, ri);
    vfs_inode_release(ri);

    pthread_mutex_lock(&g_vfs_lock);
    m->next = mount_table_head;
    mount_table_head = m;
    pthread_mutex_unlock(&g_vfs_lock);

    return m;
}

int vfs_mount_destroy(vfs_mount_entry_t *m)
{
    if (!m)
        return -EINVAL;

    pthread_mutex_lock(&g_vfs_lock);
    vfs_mount_entry_t **cur = &mount_table_head;
    while (*cur) {
        if (*cur == m) {
            *cur = m->next;
            break;
        }
        cur = &(*cur)->next;
    }
    pthread_mutex_unlock(&g_vfs_lock);

    /* Shutdown backend if present */
    if (m->backend_ops && m->backend_ops->shutdown && m->backend_data) {
        m->backend_ops->shutdown(m->backend_data);
    }

    vfs_dentry_destroy_tree(m->root_dentry);
    free(m->mountpoint);
    free(m->backend_root);
    free(m);
    return 0;
}

/* Longest-prefix match */
static vfs_mount_entry_t *find_best_mount(const char *path)
{
    vfs_mount_entry_t *best = NULL;
    size_t best_len = 0;

    pthread_mutex_lock(&g_vfs_lock);
    for (vfs_mount_entry_t *m = mount_table_head; m; m = m->next) {
        size_t ml = strlen(m->mountpoint);

        if (ml == 1 && m->mountpoint[0] == '/') {
            /* fallback root mount */
            if (!best) {
                best = m;
                best_len = 1;
            }
            continue;
        }

        if (strncmp(path, m->mountpoint, ml) == 0 &&
            (path[ml] == '\0' || path[ml] == '/'))
        {
            if (ml > best_len) {
                best = m;
                best_len = ml;
            }
        }
    }
    pthread_mutex_unlock(&g_vfs_lock);

    return best;
}

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

/* -------------------------------------------------------------------------- */
/* PATH RESOLUTION */
/* -------------------------------------------------------------------------- */

int vfs_resolve_path(const char *path, vfs_dentry_t **out)
{
    if (!path || !out)
        return -EINVAL;

    if (!g_vfs_inited)
        return -EIO;

    char *norm = normalize_path_alloc(path);
    if (!norm)
        return -EINVAL;

    vfs_mount_entry_t *m = find_best_mount(norm);
    if (!m) {
        free(norm);
        return -ENOENT;
    }

    /* Direct hit: mount root */
    if (!strcmp(norm, "/") || !strcmp(norm, m->mountpoint)) {
        *out = m->root_dentry;
        free(norm);
        return 0;
    }

    const char *rel = norm;
    size_t ml = strlen(m->mountpoint);

    if (ml > 1 && !strncmp(norm, m->mountpoint, ml)) {
        rel = (norm[ml] == '/') ? norm + ml + 1 : norm + ml;
    } else if (norm[0] == '/') {
        rel = norm + 1;
    }

    char *tmp = strdup(rel);
    if (!tmp) {
        free(norm);
        return -ENOMEM;
    }

    vfs_dentry_t *cur = m->root_dentry;

    char *save = NULL;
    char *tok = strtok_r(tmp, "/", &save);

    while (tok) {
        vfs_dentry_t *found = NULL;

        pthread_mutex_lock(&cur->lock);
        for (vfs_dentry_t *c = cur->child; c; c = c->sibling) {
            if (!strcmp(c->name, tok)) {
                found = c;
                break;
            }
        }
        pthread_mutex_unlock(&cur->lock);

        /* Auto-create */
        if (!found) {
            uint64_t ino = g_next_ino++;
            vfs_inode_t *ni = vfs_inode_create(ino, S_IFDIR | 0755, 0, 0, 0);
            vfs_dentry_t *d = vfs_dentry_create(tok, cur, ni);
            vfs_inode_release(ni);
            vfs_dentry_add_child(cur, d);
            found = d;
        }

        cur = found;
        tok = strtok_r(NULL, "/", &save);
    }

    free(tmp);
    free(norm);
    *out = cur;
    return 0;
}

/* -------------------------------------------------------------------------- */
/* PERMISSION CHECKS                                                           */
/* -------------------------------------------------------------------------- */

static int check_inode_perm(const vfs_inode_t *ino, uid_t req_uid, gid_t req_gid, int mask)
{
    if (!ino) return -ENOENT;

    /* Determine applicable class */
    int have_r = 0, have_w = 0, have_x = 0;

    if (req_uid == 0) {
        have_r = 1;
        have_w = 1;
        have_x = ((ino->mode & (S_IXUSR|S_IXGRP|S_IXOTH)) != 0);
    } else if (req_uid == ino->uid) {
        have_r = (ino->mode & S_IRUSR) != 0;
        have_w = (ino->mode & S_IWUSR) != 0;
        have_x = (ino->mode & S_IXUSR) != 0;
    } else if (req_gid == ino->gid) {
        have_r = (ino->mode & S_IRGRP) != 0;
        have_w = (ino->mode & S_IWGRP) != 0;
        have_x = (ino->mode & S_IXGRP) != 0;
    } else {
        have_r = (ino->mode & S_IROTH) != 0;
        have_w = (ino->mode & S_IWOTH) != 0;
        have_x = (ino->mode & S_IXOTH) != 0;
    }

    int need_r = (mask & R_OK) ? 1 : 0;
    int need_w = (mask & W_OK) ? 1 : 0;
    int need_x = (mask & X_OK) ? 1 : 0;

    if (need_r && !have_r) return -EACCES;
    if (need_w && !have_w) return -EACCES;
    if (need_x && !have_x) return -EACCES;
    return 0;
}

int vfs_permission_check(const char *path, uid_t uid, gid_t gid, int mask)
{
    if (!path) return -EINVAL;
    vfs_dentry_t *d = NULL;
    int ret = vfs_resolve_path(path, &d);
    if (ret != 0 || !d) return ret ? ret : -ENOENT;
    return check_inode_perm(d->inode, uid, gid, mask);
}
/* INIT + SHUTDOWN */
/* -------------------------------------------------------------------------- */

int vfs_init(void)
{
    pthread_mutex_lock(&g_vfs_lock);
    if (g_vfs_inited) {
        pthread_mutex_unlock(&g_vfs_lock);
        return 0;
    }
    g_vfs_inited = 1;
    pthread_mutex_unlock(&g_vfs_lock);

    /* Initialize file handle table */
    fh_table_init_once();

    /* Register POSIX backend */
    extern const vfs_backend_ops_t *get_posix_backend_ops(void);
    const vfs_backend_ops_t *posix_ops = get_posix_backend_ops();
    if (posix_ops) {
        int ret = vfs_register_backend(posix_ops);
        if (ret < 0 && ret != -EEXIST) {
            fprintf(stderr, "vfs_init: failed to register posix backend: %d\n", ret);
        } else {
            fprintf(stderr, "[vfs_init] POSIX backend registered successfully\n");
        }
    }

    /* Create default mount + sample tree */
    vfs_mount_entry_t *rootm = vfs_mount_create("/", ".");
    if (!rootm)
        return -ENOMEM;

    /* Some populated sample entries */
    uint64_t ino;

    ino = g_next_ino++;
    vfs_inode_t *d1i = vfs_inode_create(ino, S_IFDIR | 0755, 0, 0, 0);
    vfs_dentry_t *d1 = vfs_dentry_create("dir1", rootm->root_dentry, d1i);
    vfs_inode_release(d1i);
    vfs_dentry_add_child(rootm->root_dentry, d1);

    ino = g_next_ino++;
    vfs_inode_t *d2i = vfs_inode_create(ino, S_IFDIR | 0755, 0, 0, 0);
    vfs_dentry_t *d2 = vfs_dentry_create("dir2", d1, d2i);
    vfs_inode_release(d2i);
    vfs_dentry_add_child(d1, d2);

    ino = g_next_ino++;
    vfs_inode_t *fi = vfs_inode_create(ino, S_IFREG | 0644, 0, 0, 0);
    vfs_dentry_t *f = vfs_dentry_create("file", d2, fi);
    vfs_inode_release(fi);
    vfs_dentry_add_child(d2, f);

    ino = g_next_ino++;
    vfs_inode_t *d3i = vfs_inode_create(ino, S_IFDIR | 0755, 0, 0, 0);
    vfs_dentry_t *d3 = vfs_dentry_create("dir3", d1, d3i);
    vfs_inode_release(d3i);
    vfs_dentry_add_child(d1, d3);

    ino = g_next_ino++;
    vfs_inode_t *f2i = vfs_inode_create(ino, S_IFREG | 0644, 0, 0, 0);
    vfs_dentry_t *f2 = vfs_dentry_create("file2", d3, f2i);
    vfs_inode_release(f2i);
    vfs_dentry_add_child(d3, f2);

    return 0;
}

int vfs_shutdown(void)
{
    pthread_mutex_lock(&g_vfs_lock);

    if (!g_vfs_inited) {
        pthread_mutex_unlock(&g_vfs_lock);
        return 0;
    }

    g_vfs_inited = 0;

    while (mount_table_head) {
        vfs_mount_entry_t *m = mount_table_head;
        mount_table_head = m->next;

        /* Shutdown backend if present */
        if (m->backend_ops && m->backend_ops->shutdown && m->backend_data) {
            m->backend_ops->shutdown(m->backend_data);
        }

        vfs_dentry_destroy_tree(m->root_dentry);
        free(m->mountpoint);
        free(m->backend_root);
        free(m);
    }

    /* Clean up file handle table */
    for (int i = 0; i < VFS_MAX_FH; i++) {
        if (g_fh_table[i].in_use) {
            if (g_fh_table[i].dentry) {
                vfs_dentry_release(g_fh_table[i].dentry);
            }
            g_fh_table[i].in_use = 0;
            g_fh_table[i].dentry = NULL;
        }
    }

    pthread_mutex_unlock(&g_vfs_lock);
    return 0;
}

/* -------------------------------------------------------------------------- */
/* Public API stubs (shim implementations)                                      */
/* These return -ENOSYS for now so other layers can compile and link.         */
/* -------------------------------------------------------------------------- */

int vfs_open(const char *path, int flags)
{
    if (!path)
        return -EINVAL;

    if (!g_vfs_inited)
        return -EIO;

    /* Check if mount has backend - for O_CREAT, dispatch directly to backend */
    vfs_mount_entry_t *mount = find_best_mount(path);
    if (mount && mount->backend_ops && mount->backend_ops->open && (flags & O_CREAT)) {
        /* Creating file through backend - don't auto-create VFS dentry yet */
        char *relpath = get_relpath_for_mount(path, mount);
        if (!relpath) return -EINVAL;
        
        void *backend_handle = NULL;
        int ret = mount->backend_ops->open(mount->backend_data, relpath, flags, &backend_handle);
        free(relpath);
        
        if (ret < 0) return ret;
        
        /* Now create VFS dentry for the file */
        uint64_t ino = g_next_ino++;
        vfs_inode_t *inode = vfs_inode_create(ino, S_IFREG | 0644, 0, 0, 0);
        if (!inode) return -ENOMEM;
        inode->backend_handle = backend_handle;
        
        /* Extract filename */
        const char *name = strrchr(path, '/');
        name = name ? name + 1 : path;
        
        vfs_dentry_t *d = vfs_dentry_create(name, NULL, inode);
        vfs_inode_release(inode);
        if (!d) return -ENOMEM;
        
        /* Allocate handle */
        int fh = fh_alloc(d, flags);
        return fh;
    }

    /* Normal path: resolve and open existing file */
    vfs_dentry_t *d = NULL;
    int ret = vfs_resolve_path(path, &d);
    if (ret != 0 || !d)
        return ret ? ret : -ENOENT;

    /* Directories cannot be opened for read/write as files */
    if (d->inode && S_ISDIR(d->inode->mode))
        return -EISDIR;

    /* permission check: basic R/W */
    int mask = 0;
    if ((flags & O_WRONLY) || (flags & O_RDWR)) mask |= W_OK;
    if ((flags & O_RDONLY) || (flags & O_RDWR) || !(flags & O_WRONLY)) mask |= R_OK;

    int perm = check_inode_perm(d->inode, 0, 0, mask); /* uid=0/gid=0 default */
    if (perm != 0)
        return perm;

    /* For existing files with backend, get backend handle */
    if (mount && mount->backend_ops && mount->backend_ops->open && !d->inode->backend_handle) {
        char *relpath = get_relpath_for_mount(path, mount);
        if (!relpath) return -EINVAL;
        
        void *backend_handle = NULL;
        ret = mount->backend_ops->open(mount->backend_data, relpath, flags, &backend_handle);
        free(relpath);
        
        if (ret < 0) return ret;
        
        d->inode->backend_handle = backend_handle;
    }

    /* allocate a handle */
    int fh = fh_alloc(d, flags);
    if (fh < 0)
        return fh;

    return fh;
}

int vfs_close(int fh)
{
    vfs_fh_entry_t *e = fh_get(fh);
    if (!e)
        return -EBADF;

    fh_free(fh);
    return 0;
}

ssize_t vfs_read(int fh, void *buf, size_t count, off_t offset)
{
    if (!buf)
        return -EINVAL;

    vfs_fh_entry_t *e = fh_get(fh);
    if (!e)
        return -EBADF;

    vfs_dentry_t *d = e->dentry;
    if (!d || !d->inode)
        return -ENOENT;

    if (S_ISDIR(d->inode->mode))
        return -EISDIR;

    /* permission check: read */
    int perm = check_inode_perm(d->inode, 0, 0, R_OK);
    if (perm != 0)
        return perm;

    /* Check if backend handle is available */
    if (d->inode->backend_handle) {
        /* Find mount to get backend ops - simplified: we know handle exists so backend must */
        /* We could store mount ref in dentry, but for now iterate mounts */
        pthread_mutex_lock(&g_vfs_lock);
        for (vfs_mount_entry_t *m = mount_table_head; m; m = m->next) {
            if (m->backend_ops && m->backend_ops->read) {
                ssize_t result = m->backend_ops->read(m->backend_data, 
                                                      d->inode->backend_handle,
                                                      buf, count, offset);
                pthread_mutex_unlock(&g_vfs_lock);
                return result;
            }
        }
        pthread_mutex_unlock(&g_vfs_lock);
    }

    /* Fallback: simple zero-filled content model */
    off_t size = d->inode->size;
    if (offset >= size)
        return 0;

    size_t avail = (size - offset) > 0 ? (size - offset) : 0;
    size_t n = (count < avail) ? count : avail;
    memset(buf, 0, n);
    return (ssize_t)n;
}

ssize_t vfs_write(int fh, const void *buf, size_t count, off_t offset)
{
    if (!buf)
        return -EINVAL;

    vfs_fh_entry_t *e = fh_get(fh);
    if (!e)
        return -EBADF;

    vfs_dentry_t *d = e->dentry;
    if (!d || !d->inode)
        return -ENOENT;

    if (S_ISDIR(d->inode->mode))
        return -EISDIR;

    /* permission check: write */
    int perm = check_inode_perm(d->inode, 0, 0, W_OK);
    if (perm != 0)
        return perm;

    /* Check if backend handle is available */
    if (d->inode->backend_handle) {
        pthread_mutex_lock(&g_vfs_lock);
        for (vfs_mount_entry_t *m = mount_table_head; m; m = m->next) {
            if (m->backend_ops && m->backend_ops->write) {
                ssize_t written = m->backend_ops->write(m->backend_data,
                                                         d->inode->backend_handle,
                                                         buf, count, offset);
                pthread_mutex_unlock(&g_vfs_lock);
                if (written > 0) {
                    /* Update size */
                    off_t new_size = offset + written;
                    if (new_size > d->inode->size)
                        d->inode->size = new_size;
                }
                return written;
            }
        }
        pthread_mutex_unlock(&g_vfs_lock);
    }

    /* Fallback: Grow file size to simulate write */
    off_t end = offset + (off_t)count;
    if (end > d->inode->size)
        d->inode->size = end;

    /* We don't store content; pretend we wrote all bytes */
    return (ssize_t)count;
}

int vfs_stat(const char *path, struct stat *st)
{
    if (!path || !st)
        return -EINVAL;

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
    if (ret != 0 || !d)
        return ret ? ret : -ENOENT;

    if (!d->inode)
        return -ENOENT;

    memset(st, 0, sizeof(*st));
    st->st_mode = d->inode->mode;
    st->st_size = d->inode->size;
    st->st_uid = d->inode->uid;
    st->st_gid = d->inode->gid;
    st->st_ino = d->inode->ino;
    return 0;
}

int vfs_readdir(const char *path, void *buf, void *filler, off_t offset, void *fi)
{
    (void)offset;  /* Simple implementation - ignore offset */
    (void)fi;      /* Not using file info */
    
    if (!path || !buf || !filler)
        return -EINVAL;

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
    if (ret != 0 || !d)
        return ret ? ret : -ENOENT;

    if (!d->inode)
        return -ENOENT;

    if (!S_ISDIR(d->inode->mode))
        return -ENOTDIR;

    /* FUSE3 filler: int (*)(void *buf, const char *name, const struct stat *st, off_t off, enum flags) */
    typedef int (*fill_fn_t)(void *, const char *, const struct stat *, off_t, int);
    fill_fn_t fill = (fill_fn_t)filler;

    /* Add . and .. entries (pass 0 for flags) */
    if (fill(buf, ".", NULL, 0, 0) != 0) {
        /* Buffer full on first entry - this shouldn't happen */
        return -EIO;
    }
    fill(buf, "..", NULL, 0, 0);

    /* Add child entries */
    pthread_mutex_lock(&d->lock);
    for (vfs_dentry_t *c = d->child; c; c = c->sibling) {
        if (fill(buf, c->name, NULL, 0, 0) != 0) {
            /* Buffer full - stop adding but return success */
            break;
        }
    }
    pthread_mutex_unlock(&d->lock);

    return 0;
}

/* vfs_permission_check: already implemented above; remove stub duplicate. */

/* Public wrapper: vfs_lookup delegates to vfs_resolve_path. */
int vfs_lookup(const char *path, vfs_dentry_t **out)
{
    if (!path || !out)
        return -EINVAL;

    return vfs_resolve_path(path, out);
}

/* -------------------------------------------------------------------------- */
/* Public Mount API                                                           */
/* -------------------------------------------------------------------------- */

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

int vfs_unmount_backend(const char *mountpoint)
{
    if (!mountpoint)
        return -EINVAL;

    pthread_mutex_lock(&g_vfs_lock);
    vfs_mount_entry_t *m = NULL;
    for (vfs_mount_entry_t *cur = mount_table_head; cur; cur = cur->next) {
        if (strcmp(cur->mountpoint, mountpoint) == 0) {
            m = cur;
            break;
        }
    }
    pthread_mutex_unlock(&g_vfs_lock);

    if (!m)
        return -ENOENT;

    return vfs_mount_destroy(m);
}

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
    
    /* Use open with O_CREAT flag */
    int flags = O_CREAT | O_EXCL | O_RDWR;
    return vfs_open(path, flags | mode);
}

int vfs_mkdir(const char *path, mode_t mode) {
    if (!path) return -EINVAL;
    
    /* Create directory in VFS tree */
    vfs_dentry_t *d = NULL;
    int ret = vfs_resolve_path(path, &d);
    if (ret < 0) return ret;
    
    /* If already exists, return error */
    if (d && d->inode) return -EEXIST;
    
    /* Auto-creation by resolve_path already made it as directory */
    /* Just verify it exists now */
    if (d && d->inode && S_ISDIR(d->inode->mode)) {
        return 0;
    }
    
    return -EIO;
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
