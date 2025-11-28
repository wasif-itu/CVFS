#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "vfs_core.h"

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

    // Cleanup - vfs_shutdown handles dentry tree cleanup
    vfs_shutdown();
    return 0;
}
