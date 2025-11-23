#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vfs_core.h"

// Simple helper to cleanup a tree recursively
static void free_dentry_tree(vfs_dentry_t *d) {
    if (!d) return;

    pthread_mutex_lock(&d->lock);
    vfs_dentry_t *child = d->child;
    while (child) {
        vfs_dentry_t *next = child->sibling;
        free_dentry_tree(child);
        child = next;
    }
    pthread_mutex_unlock(&d->lock);

    vfs_dentry_release(d->inode);
    free(d->name);
    pthread_mutex_destroy(&d->lock);
    free(d);
}

int main() {
    printf("Running vfs_resolve_path() tests...\n");

    vfs_init();

    vfs_dentry_t *dentry = NULL;
    int ret;

    // Test root path
    ret = vfs_resolve_path("/", &dentry);
    if (ret != 0 || !dentry) { printf("FAIL root path\n"); return 1; }

    // Test single-level path
    ret = vfs_resolve_path("/dir1", &dentry);
    if (ret != 0 || !dentry || strcmp(dentry->name, "dir1") != 0) {
        printf("FAIL /dir1\n"); return 1;
    }

    // Test nested path
    ret = vfs_resolve_path("/dir1/dir2/file", &dentry);
    if (ret != 0 || !dentry || strcmp(dentry->name, "file") != 0) {
        printf("FAIL /dir1/dir2/file\n"); return 1;
    }

    // Test path normalization
    ret = vfs_resolve_path("/dir1//dir2/../dir3/./file2", &dentry);
    if (ret != 0 || !dentry || strcmp(dentry->name, "file2") != 0) {
        printf("FAIL path normalization\n"); return 1;
    }

    printf("All tests passed!\n");

    // Cleanup
    for (mount_entry_t *m = mount_table_head; m != NULL; m = m->next) {
        free_dentry_tree(m->root_dentry);
        m->root_dentry = NULL;
    }

    vfs_shutdown();
    return 0;
}
