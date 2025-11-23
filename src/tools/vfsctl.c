#include <stdio.h>
#include <stdlib.h>
#include "../core/vfs_core.h"

int main(int argc, char **argv) {
    (void)argc; (void)argv;

    if (vfs_init() != 0) {
        fprintf(stderr, "vfs_init failed\n");
        return 1;
    }

    printf("vfsctl: startup OK\n");

    vfs_shutdown();
    return 0;
}
