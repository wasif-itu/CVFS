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

/* -------------------------------------------------------------------------- */
/* GLOBAL STATE */
/* -------------------------------------------------------------------------- */
static pthread_mutex_t g_vfs_lock = PTHREAD_MUTEX_INITIALIZER;

vfs_mount_entry_t *mount_table_head = NULL;
static uint64_t g_next_ino = 1000;
static int g_vfs_inited = 0;

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

void vfs_dentry_release(vfs_inode_t *inode)
{
    /* Small wrapper to match older test expectations */
    vfs_inode_release(inode);
}

static void destroy_dentry_only(vfs_dentry_t *d)
{
    pthread_mutex_destroy(&d->lock);
    if (d->inode)
        vfs_inode_release(d->inode);
    free(d->name);
    free(d);
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
        vfs_inode_t *ino = m->root_dentry->inode;
        vfs_inode_acquire(ino);
        *out = vfs_dentry_create("/", NULL, ino);
        vfs_inode_release(ino);
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

        vfs_dentry_destroy_tree(m->root_dentry);
        free(m->mountpoint);
        free(m->backend_root);
        free(m);
    }

    pthread_mutex_unlock(&g_vfs_lock);
    return 0;
}
