
/* ================================================================
 * FILE: cache/cache.h
 * ================================================================ */
#ifndef CACHE_H
#define CACHE_H

#include <stdint.h>
#include <stddef.h>
#include "cache_entry.h"

typedef struct Cache {
    CacheEntry **table;
    size_t capacity;        // Maximum number of entries
    size_t current_size;    // Current number of entries
    size_t table_size;      // Hash table size (buckets)
    uint64_t tau;           // Working-set window size (W)
} Cache;

// Core cache operations
void cache_init(size_t capacity, uint64_t tau);
void cache_shutdown(void);

uint8_t *cache_lookup(uint64_t block_id, size_t *size);
void cache_insert(uint64_t block_id, uint8_t *data, size_t size);

void cache_update_access(CacheEntry *entry);
void cache_evict_if_needed(void);

// Statistics (optional)
void cache_print_stats(void);

#endif // CACHE_H
