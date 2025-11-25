#define _GNU_SOURCE
#include <errno.h>
#include <inttypes.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include "backend_posix.h"

static int filler_print(void *buf, const char *name, const struct stat *st, off_t off) {
    (void)buf; (void)st; (void)off;
    printf("entry: %s\n", name);
    return 0;
}

int main(void) {
    const char *root = "/tmp/cvfs_test_backend_io";
    if (mkdir(root, 0755) != 0 && errno != EEXIST) {
        perror("mkdir");
        return 2;
    }

    int id = posix_backend_init(root);
    if (id < 1) { perror("posix_backend_init"); return 3; }

    /* create and open file */
    const char *rel = "hello.txt";
    int h = posix_open(id, rel, O_CREAT | O_RDWR, 0644);
    if (h < 0) { perror("posix_open"); posix_backend_shutdown(id); return 4; }

    const char *data = "Hello CVFS!\n";
    ssize_t w = posix_write(id, h, data, strlen(data), 0);
    if (w < 0) { perror("posix_write"); posix_close(id, h); posix_backend_shutdown(id); return 5; }
    printf("wrote %zd bytes\n", w);

    /* read back */
    char buf[64] = {0};
    ssize_t r = posix_read(id, h, buf, sizeof(buf)-1, 0);
    if (r < 0) { perror("posix_read"); posix_close(id, h); posix_backend_shutdown(id); return 6; }
    printf("read %zd bytes: %s", r, buf);

    /* stat */
    struct stat st;
    if (posix_stat(id, rel, &st) != 0) { perror("posix_stat"); posix_close(id, h); posix_backend_shutdown(id); return 7; }
    printf("stat: size=%jd\n", (intmax_t)st.st_size);

    /* close */
    if (posix_close(id, h) != 0) { perror("posix_close"); posix_backend_shutdown(id); return 8; }

    /* readdir on root */
    printf("readdir results:\n");
    if (posix_readdir(id, ".", NULL, (vfs_fill_dir_t)filler_print, 0) != 0) {
        perror("posix_readdir");
        posix_backend_shutdown(id);
        return 9;
    }

    if (posix_backend_shutdown(id) != 0) { perror("posix_backend_shutdown"); return 10; }

    printf("ALL backend IO TESTS PASSED\n");
    return 0;
}
