#include "../src/core/vfs_core.h"
#include "../src/backends/backend_posix.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>

int main(void) {
    printf("=== VFS Integration Test ===\n\n");
    
    /* Initialize VFS */
    printf("1. Initializing VFS...\n");
    int ret = vfs_init();
    if (ret != 0) {
        fprintf(stderr, "FAIL: vfs_init returned %d\n", ret);
        return 1;
    }
    printf("   ✓ VFS initialized\n\n");
    
    /* Create test directory */
    printf("2. Creating test backend directory...\n");
    system("mkdir -p /tmp/vfs_test_backend");
    printf("   ✓ Created /tmp/vfs_test_backend\n\n");
    
    /* Mount POSIX backend */
    printf("3. Mounting POSIX backend at /backend...\n");
    ret = vfs_mount_backend("/backend", "/tmp/vfs_test_backend", "posix");
    if (ret != 0) {
        fprintf(stderr, "FAIL: vfs_mount_backend returned %d\n", ret);
        vfs_shutdown();
        return 1;
    }
    printf("   ✓ Backend mounted\n\n");
    
    /* Test file creation through backend */
    printf("4. Creating test file /backend/test.txt...\n");
    int fh = vfs_open("/backend/test.txt", O_CREAT | O_RDWR);
    if (fh < 0) {
        fprintf(stderr, "FAIL: vfs_open returned %d\n", fh);
        vfs_shutdown();
        return 1;
    }
    printf("   ✓ File created (handle %d)\n\n", fh);
    
    /* Write data */
    printf("5. Writing data to file...\n");
    const char *data = "Hello VFS Integration!\n";
    ssize_t written = vfs_write(fh, data, strlen(data), 0);
    if (written < 0) {
        fprintf(stderr, "FAIL: vfs_write returned %ld\n", (long)written);
        vfs_close(fh);
        vfs_shutdown();
        return 1;
    }
    printf("   ✓ Wrote %ld bytes\n\n", (long)written);
    
    /* Read data back */
    printf("6. Reading data from file...\n");
    char buf[256] = {0};
    ssize_t nread = vfs_read(fh, buf, sizeof(buf) - 1, 0);
    if (nread < 0) {
        fprintf(stderr, "FAIL: vfs_read returned %ld\n", (long)nread);
        vfs_close(fh);
        vfs_shutdown();
        return 1;
    }
    buf[nread] = '\0';
    printf("   ✓ Read %ld bytes: \"%s\"\n\n", (long)nread, buf);
    
    /* Verify data */
    if (strcmp(buf, data) != 0) {
        fprintf(stderr, "FAIL: Data mismatch!\n");
        fprintf(stderr, "  Expected: \"%s\"\n", data);
        fprintf(stderr, "  Got:      \"%s\"\n", buf);
        vfs_close(fh);
        vfs_shutdown();
        return 1;
    }
    printf("   ✓ Data verified\n\n");
    
    /* Close file */
    printf("7. Closing file...\n");
    ret = vfs_close(fh);
    if (ret < 0) {
        fprintf(stderr, "FAIL: vfs_close returned %d\n", ret);
        vfs_shutdown();
        return 1;
    }
    printf("   ✓ File closed\n\n");
    
    /* Test stat */
    printf("8. Testing stat on /backend/test.txt...\n");
    struct stat st;
    ret = vfs_stat("/backend/test.txt", &st);
    if (ret != 0) {
        fprintf(stderr, "FAIL: vfs_stat returned %d\n", ret);
        vfs_shutdown();
        return 1;
    }
    printf("   ✓ Stat: size=%ld mode=0%o\n\n", (long)st.st_size, st.st_mode & 0777);
    
    /* Cleanup */
    printf("9. Shutting down VFS...\n");
    ret = vfs_shutdown();
    if (ret != 0) {
        fprintf(stderr, "FAIL: vfs_shutdown returned %d\n", ret);
        return 1;
    }
    printf("   ✓ VFS shutdown\n\n");
    
    printf("=== ALL INTEGRATION TESTS PASSED ===\n");
    return 0;
}
