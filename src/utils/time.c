

/* ================================================================
 * FILE: utils/time.c
 * ================================================================ */
#include "time.h"

static uint64_t global_time = 0;

uint64_t vfs_time_now(void) {
    return ++global_time;
}

void vfs_time_reset(void) {
    global_time = 0;
}

