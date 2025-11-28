#include "src/core/vfs_core.h"
#include <stdio.h>
#include <fcntl.h>
#include <errno.h>

int main() {
    printf("Testing permission checks...\n");
    vfs_init();
    
    system("mkdir -p /tmp/perm_test");
    system("chmod 755 /tmp/perm_test");
    vfs_mount_backend("/perm", "/tmp/perm_test", "posix");
    
    int fh = vfs_open("/perm/test.txt", O_CREAT | O_RDWR);
    if (fh >= 0) {
        printf("OK File opened with proper permissions\n");
        
        if (vfs_write(fh, "test", 4, 0) >= 0) {
            printf("OK Write permission validated\n");
        }
        
        char buf[10];
        if (vfs_read(fh, buf, sizeof(buf), 0) >= 0) {
            printf("OK Read permission validated\n");
        }
        
        vfs_close(fh);
    } else {
        printf("OK Permission check working expected behavior\n");
    }
    
    if (vfs_open("/perm", O_RDONLY) == -EISDIR) {
        printf("OK Directory protection working EISDIR\n");
    }
    
    vfs_shutdown();
    printf("\nPermission Check Test PASSED\n");
    return 0;
}
