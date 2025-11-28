#include "src/core/vfs_core.h"
#include <stdio.h>
#include <string.h>
#include <fcntl.h>

int main() {
    printf("Testing backend dispatch mechanism...\n");
    vfs_init();
    
    system("rm -rf /tmp/vfs_dispatch_test");
    system("mkdir -p /tmp/vfs_dispatch_test");
    
    if (vfs_mount_backend("/test", "/tmp/vfs_dispatch_test", "posix") != 0) {
        printf("FAIL: Mount failed\n");
        return 1;
    }
    printf("OK Backend mounted\n");
    
    int fh = vfs_open("/test/dispatch.txt", O_CREAT | O_RDWR);
    if (fh < 0) {
        printf("FAIL: Open failed\n");
        return 1;
    }
    printf("OK File created handle %d\n", fh);
    
    const char *data = "Backend dispatch test data";
    ssize_t written = vfs_write(fh, data, strlen(data), 0);
    if (written != (ssize_t)strlen(data)) {
        printf("FAIL: Write failed\n");
        return 1;
    }
    printf("OK Wrote %zd bytes\n", written);
    
    char buf[100] = {0};
    ssize_t nread = vfs_read(fh, buf, sizeof(buf), 0);
    if (nread != written || strcmp(buf, data) != 0) {
        printf("FAIL: Read verification failed\n");
        return 1;
    }
    printf("OK Read %zd bytes data matches\n", nread);
    
    vfs_close(fh);
    vfs_shutdown();
    
    printf("\nBackend Dispatch Test PASSED\n");
    return 0;
}
