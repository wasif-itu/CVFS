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

int main(void) {
    const char *root = "/tmp/cvfs_test_backend_modify";
    if (mkdir(root, 0755) != 0 && errno != EEXIST) {
        perror("mkdir");
        return 2;
    }

    int id = posix_backend_init(root);
    if (id < 1) { perror("posix_backend_init"); return 3; }

    /* create file */
    const char *file = "file.txt";
    int h = posix_create(id, file, 0644);
    if (h < 0) { perror("posix_create"); posix_backend_shutdown(id); return 4; }
    printf("created file handle=%d\n", h);

    const char *msg = "modify-test\n";
    if (posix_write(id, h, msg, strlen(msg), 0) < 0) { perror("posix_write"); posix_close(id, h); posix_backend_shutdown(id); return 5; }

    /* stat */
    struct stat st;
    if (posix_stat(id, file, &st) != 0) { perror("posix_stat"); posix_close(id, h); posix_backend_shutdown(id); return 6; }
    printf("stat size=%jd\n", (intmax_t)st.st_size);

    /* close */
    if (posix_close(id, h) != 0) { perror("posix_close"); posix_backend_shutdown(id); return 7; }

    /* rename */
    if (posix_rename(id, file, "file2.txt") != 0) { perror("posix_rename"); posix_backend_shutdown(id); return 8; }
    printf("rename OK\n");

    /* mkdir */
    if (posix_mkdir(id, "subdir", 0755) != 0) { perror("posix_mkdir"); posix_backend_shutdown(id); return 9; }
    printf("mkdir OK\n");

    /* unlink file2 */
    if (posix_unlink(id, "file2.txt") != 0) { perror("posix_unlink"); posix_backend_shutdown(id); return 10; }
    printf("unlink OK\n");

    /* cleanup */
    if (posix_unlink(id, "subdir") != 0) { /* may rmdir via our unlink wrapper */ perror("posix_unlink subdir"); /* not fatal */ }

    if (posix_backend_shutdown(id) != 0) { perror("posix_backend_shutdown"); return 11; }

    printf("ALL modify tests PASSED\n");
    return 0;
}
