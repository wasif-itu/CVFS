#include <stdio.h>
#include <assert.h>
#include <stdlib.h>
#include <string.h>
#include "vfs_core.h"

/* Simple test: create/destroy inodes + dentries and check refcounts.
   Build with: gcc tests/test_core_structs.c src/core/vfs_core.c -o tests/test_core_structs -pthread
*/

static void test_inode_refcount(void) {
    vfs_inode_t *i = vfs_inode_create(1, 0644, 1000, 1000, 0);
    assert(i != NULL);
    /* initial refcount == 1 */
    pthread_mutex_lock(&i->lock);
    int rc = i->refcount;
    pthread_mutex_unlock(&i->lock);
    printf("inode initial refcount = %d\n", rc);
    assert(rc == 1);

    vfs_inode_acquire(i);
    vfs_inode_acquire(i);

    pthread_mutex_lock(&i->lock);
    rc = i->refcount;
    pthread_mutex_unlock(&i->lock);
    printf("inode after 2 acquires refcount = %d\n", rc);
    assert(rc == 3);

    vfs_inode_release(i);
    vfs_inode_release(i);

    /* after two releases, back to 1 */
    /* manually check */
    vfs_inode_release(i); /* final release: should free and not crash */
    printf("inode released completely (no crash)\n");
}

static void test_dentry_tree(void) {
    /* create root inode and dentry */
    vfs_inode_t *root_inode = vfs_inode_create(2, S_IFDIR | 0755, 0, 0, 0);
    assert(root_inode);

    vfs_dentry_t *root = vfs_dentry_create("/", NULL, root_inode);
    assert(root);
    vfs_inode_release(root_inode);  /* dentry holds a reference now */
    
    /* create child entries */
    vfs_inode_t *a_inode = vfs_inode_create(3, S_IFDIR | 0755, 1000, 1000, 0);
    vfs_dentry_t *a = vfs_dentry_create("a", root, a_inode);
    assert(a);
    vfs_inode_release(a_inode);  /* dentry holds a reference now */
    
    vfs_inode_t *b_inode = vfs_inode_create(4, S_IFREG | 0644, 1000, 1000, 10);
    vfs_dentry_t *b = vfs_dentry_create("b", root, b_inode);
    assert(b);
    vfs_inode_release(b_inode);  /* dentry holds a reference now */

    /* basic sanity */
    assert(strcmp(root->name, "/") == 0);
    assert(a->parent == root);
    assert(b->parent == root);

    /* destroy in reverse order */
    vfs_dentry_destroy(b);
    vfs_dentry_destroy(a);
    vfs_dentry_destroy(root);
    /* all releases should have freed inodes without double-freeing */
    printf("dentry tree created and destroyed successfully\n");
}

int main(void) {
    if (vfs_init() != 0) {
        fprintf(stderr, "vfs_init failed\n");
        return 1;
    }

    test_inode_refcount();
    test_dentry_tree();

    if (vfs_shutdown() != 0) {
        fprintf(stderr, "vfs_shutdown failed\n");
        return 1;
    }

    printf("ALL TESTS PASSED\n");
    return 0;
}
