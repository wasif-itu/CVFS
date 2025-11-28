#include "src/core/vfs_core.h"
#include <stdio.h>

int main() {
    printf("Testing backend registry...\n");
    
    if (vfs_init() != 0) {
        printf("FAIL: Init failed\n");
        return 1;
    }
    printf("OK VFS initialized POSIX backend auto-registered\n");
    
    system("mkdir -p /tmp/registry_test");
    if (vfs_mount_backend("/reg", "/tmp/registry_test", "posix") == 0) {
        printf("OK POSIX backend found and mounted\n");
    } else {
        printf("FAIL: Backend mount failed\n");
        return 1;
    }
    
    if (vfs_mount_backend("/invalid", "/tmp/test", "nonexistent") < 0) {
        printf("OK Invalid backend correctly rejected\n");
    } else {
        printf("FAIL: Should reject invalid backend\n");
        return 1;
    }
    
    vfs_shutdown();
    printf("\nBackend Registry Test PASSED\n");
    return 0;
}
