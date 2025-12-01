
/* ================================================================
 * FILE: cache/working_set.h
 * ================================================================ */
#ifndef WORKING_SET_H
#define WORKING_SET_H

#include <stdint.h>
#include "cache_entry.h"

uint64_t ws_current_time(void);
int ws_is_in_working_set(CacheEntry *entry, uint64_t now, uint64_t tau);

#endif // WORKING_SET_H
