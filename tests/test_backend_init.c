#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include "backend_posix.h"

int main(void) {
    const char *root = "/tmp/cvfs_test_backend";
    /* ensure directory exists */
    if (mkdir(root, 0755) != 0 && errno != EEXIST) {
        perror("mkdir");
        return 2;
    }

    int id = posix_backend_init(root);
    if (id < 1) {
        perror("posix_backend_init");
        return 3;
    }
    printf("backend id = %d\n", id);

    if (posix_backend_shutdown(id) != 0) {
        perror("posix_backend_shutdown");
        return 4;
    }
    printf("shutdown OK\n");
    return 0;
}
