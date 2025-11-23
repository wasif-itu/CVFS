#ifndef VFS_CORE_H
#define VFS_CORE_H

#include <stddef.h>
#include <stdint.h>
#include <sys/stat.h>
#include <unistd.h>
#include <pthread.h>

/*
 * ================================
 *  VFS CORE — PUBLIC INTERFACE
 * ================================
 *
 * Clean, stable header for Day 1–3 implementation.
 * No duplicates, no conflicting typedefs.
 */

/* Forward declarations */
struct vfs_dentry;
struct vfs_mount;

/* ----------------------------------
 * Inode structure
 * ---------------------------------- */
typedef struct vfs_inode {
    uint64_t        ino;            /* inode number */
    mode_t          mode;
    uid_t           uid;
    gid_t           gid;
    off_t           size;

    int             refcount;       /* protected by lock */
    void           *backend_handle; /* opaque backend data */

    pthread_mutex_t lock;           /* protects inode fields */
} vfs_inode_t;

/* ----------------------------------
 * Dentry structure
 * ---------------------------------- */
typedef struct vfs_dentry {
    char *name;
    struct vfs_dentry *parent;
    vfs_inode_t *inode;
    pthread_mutex_t lock;

    /* Child pointers (simple singly-linked tree) */
    struct vfs_dentry *child;   /* first child */
    struct vfs_dentry *sibling; /* next sibling */
} vfs_dentry_t;

/* ----------------------------------
 * Mount entry — linked list node
 * ---------------------------------- */
typedef struct vfs_mount {
    char *mountpoint;            /* e.g. "/mnt" */
    char *backend_root;          /* physical path on host fs */
    void *backend_ops;           /* backend function table (unused for now) */

    vfs_dentry_t *root_dentry;   /* root of mount */

    struct vfs_mount *next;      /* linked list pointer */
} vfs_mount_entry_t;

/* Global mount list */
extern vfs_mount_entry_t *mount_table_head;

/* ----------------------------------
 * Initialization
 * ---------------------------------- */
int vfs_init(void);
int vfs_shutdown(void);

/* ----------------------------------
 * Mount helpers
 * ---------------------------------- */
vfs_mount_entry_t* vfs_mount_create(const char *mountpoint,
                                    const char *backend_root);
int vfs_mount_destroy(vfs_mount_entry_t *m);

/* ----------------------------------
 * Inode helpers
 * ---------------------------------- */
vfs_inode_t* vfs_inode_create(uint64_t ino, mode_t mode,
                              uid_t uid, gid_t gid, off_t size);
void vfs_inode_acquire(vfs_inode_t *inode);
void vfs_inode_release(vfs_inode_t *inode);

/* Convenience alias used by some tests: release an inode via dentry API */
void vfs_dentry_release(vfs_inode_t *inode);

/* ----------------------------------
 * Dentry helpers
 * ---------------------------------- */
vfs_dentry_t* vfs_dentry_create(const char *name,
                                vfs_dentry_t *parent,
                                vfs_inode_t *inode);

void vfs_dentry_destroy(vfs_dentry_t *dentry);

void vfs_dentry_add_child(vfs_dentry_t *parent, vfs_dentry_t *child);
void vfs_dentry_remove_child(vfs_dentry_t *parent, vfs_dentry_t *child);

void vfs_dentry_destroy_tree(vfs_dentry_t *root);

/* ----------------------------------
 * Path resolution (Day 3)
 * ---------------------------------- */
/*
 * vfs_resolve_path():
 *   - pure in-memory resolution
 *   - normalizes path
 *   - matches longest mountpoint prefix
 *   - walks/creates dentries
 *   - returns final dentry
 */
int vfs_resolve_path(const char *path, vfs_dentry_t **out);

/*
 * vfs_lookup():
 *   simple wrapper: returns 0 if found, -ENOENT if not
 */
int vfs_lookup(const char *path, vfs_dentry_t **out);

#endif /* VFS_CORE_H */
