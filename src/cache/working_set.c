
/* ================================================================
 * FILE: cache/working_set.c
 * ================================================================ */
#include "working_set.h"
#include "../utils/time.h"

uint64_t ws_current_time(void) {
    return vfs_time_now();
}

int ws_is_in_working_set(CacheEntry *entry, uint64_t now, uint64_t tau) {
    if (entry == NULL) {
        return 0;
    }
    return (now - entry->last_access_time) <= tau;
}
