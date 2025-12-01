/* ================================================================
 * VFS CACHE SYSTEM - COMPLETE IMPLEMENTATION
 * Working-Set Model Based Cache
 * ================================================================ */

/* ================================================================
 * FILE: cache/cache_entry.h
 * ================================================================ */
#ifndef CACHE_ENTRY_H
#define CACHE_ENTRY_H

#include <stdint.h>
#include <stddef.h>

typedef struct CacheEntry {
    uint64_t block_id;
    uint8_t *data;
    size_t size;

    // Working Set Model tracking
    uint64_t last_access_time;
    uint64_t ref_count;

    struct CacheEntry *next;  // For hash table chaining
} CacheEntry;

#endif // CACHE_ENTRY_H





// /* ================================================================
//  * FILE: core/vfs.h
//  * ================================================================ */
// #ifndef VFS_H
// #define VFS_H

// #include <stdint.h>
// #include <stddef.h>

// // VFS initialization
// void vfs_init(size_t cache_capacity, uint64_t cache_tau);
// void vfs_shutdown(void);

// // Block-level I/O
// uint8_t *vfs_read_block(uint64_t block_id, size_t *size);
// void vfs_write_block(uint64_t block_id, uint8_t *data, size_t size);

// // Disk operations (to be implemented by actual disk driver)
// uint8_t *disk_read_block(uint64_t block_id, size_t *size);
// void disk_write_block(uint64_t block_id, uint8_t *data, size_t size);

// #endif // VFS_H



// /* ================================================================
//  * FILE: core/vfs.c
//  * ================================================================ */
// #include "vfs.h"
// #include "../cache/cache.h"
// #include <stdlib.h>
// #include <string.h>
// #include <stdio.h>

// // Simulated disk storage (for demonstration)
// #define DISK_BLOCKS 1024
// #define BLOCK_SIZE 4096

// static uint8_t *simulated_disk[DISK_BLOCKS];

// void vfs_init(size_t cache_capacity, uint64_t cache_tau) {
//     // Initialize simulated disk
//     for (int i = 0; i < DISK_BLOCKS; i++) {
//         simulated_disk[i] = NULL;
//     }

//     // Initialize cache
//     cache_init(cache_capacity, cache_tau);
    
//     printf("VFS initialized with cache (capacity=%zu, tau=%lu)\n", 
//            cache_capacity, cache_tau);
// }

// void vfs_shutdown(void) {
//     // Clean up simulated disk
//     for (int i = 0; i < DISK_BLOCKS; i++) {
//         if (simulated_disk[i] != NULL) {
//             free(simulated_disk[i]);
//             simulated_disk[i] = NULL;
//         }
//     }

//     cache_shutdown();
//     printf("VFS shutdown complete\n");
// }

// uint8_t *disk_read_block(uint64_t block_id, size_t *size) {
//     if (block_id >= DISK_BLOCKS) {
//         fprintf(stderr, "Block ID %lu out of range\n", block_id);
//         return NULL;
//     }

//     if (simulated_disk[block_id] == NULL) {
//         // Block doesn't exist - return zeros
//         simulated_disk[block_id] = (uint8_t *)calloc(BLOCK_SIZE, 1);
//     }

//     if (size != NULL) {
//         *size = BLOCK_SIZE;
//     }

//     // Return a copy
//     uint8_t *data = (uint8_t *)malloc(BLOCK_SIZE);
//     if (data != NULL) {
//         memcpy(data, simulated_disk[block_id], BLOCK_SIZE);
//     }
    
//     return data;
// }

// void disk_write_block(uint64_t block_id, uint8_t *data, size_t size) {
//     if (block_id >= DISK_BLOCKS || data == NULL) {
//         fprintf(stderr, "Invalid disk write parameters\n");
//         return;
//     }

//     if (simulated_disk[block_id] == NULL) {
//         simulated_disk[block_id] = (uint8_t *)malloc(BLOCK_SIZE);
//     }

//     if (simulated_disk[block_id] != NULL) {
//         size_t copy_size = (size < BLOCK_SIZE) ? size : BLOCK_SIZE;
//         memcpy(simulated_disk[block_id], data, copy_size);
//     }
// }

// uint8_t *vfs_read_block(uint64_t block_id, size_t *size) {
//     // Try cache first
//     uint8_t *data = cache_lookup(block_id, size);
//     if (data != NULL) {
//         // Cache hit - return cached data (make a copy for safety)
//         uint8_t *copy = (uint8_t *)malloc(*size);
//         if (copy != NULL) {
//             memcpy(copy, data, *size);
//         }
//         return copy;
//     }

//     // Cache miss - read from disk
//     data = disk_read_block(block_id, size);
//     if (data != NULL) {
//         // Insert into cache
//         cache_insert(block_id, data, *size);
//     }

//     return data;
// }

// void vfs_write_block(uint64_t block_id, uint8_t *data, size_t size) {
//     if (data == NULL || size == 0) {
//         return;
//     }

//     // Write to disk
//     disk_write_block(block_id, data, size);

//     // Update cache
//     cache_insert(block_id, data, size);
// }





// /* ================================================================
//  * FILE: main.c - DEMONSTRATION
//  * ================================================================ */
// #include <stdio.h>
// #include <string.h>
// #include "core/vfs.h"
// #include "core/file.h"
// #include "cache/cache.h"

// int main(void) {
//     printf("=== VFS Cache System Demo ===\n\n");

//     // Initialize VFS with cache (capacity=10, tau=5)
//     vfs_init(10, 5);

//     // Test 1: Write and read blocks
//     printf("Test 1: Block I/O with caching\n");
//     const char *test_data = "Hello, VFS Cache!";
//     vfs_write_block(100, (uint8_t *)test_data, strlen(test_data) + 1);
    
//     size_t size;
//     uint8_t *read_data = vfs_read_block(100, &size);
//     printf("  Read from cache: %s\n", read_data);
//     free(read_data);

//     // Test 2: Multiple accesses (working set test)
//     printf("\nTest 2: Working set behavior\n");
//     for (int i = 0; i < 15; i++) {
//         char buf[64];
//         snprintf(buf, sizeof(buf), "Block %d data", i);
//         vfs_write_block(i, (uint8_t *)buf, strlen(buf) + 1);
//     }

//     cache_print_stats();

//     // Test 3: Cache hits on recent blocks
//     printf("\nTest 3: Cache hit test\n");
//     for (int i = 10; i < 15; i++) {
//         read_data = vfs_read_block(i, &size);
//         printf("  Block %d: %s\n", i, read_data);
//         free(read_data);
//     }

//     // Test 4: File operations
//     printf("\nTest 4: File operations\n");
//     File *file = file_open(200);
//     if (file != NULL) {
//         const char *file_data = "File content test";
//         file_write(file, (uint8_t *)file_data, strlen(file_data) + 1);
        
//         file->position = 0;
//         uint8_t read_buf[128];
//         size_t bytes_read = file_read(file, read_buf, sizeof(read_buf));
//         printf("  File read (%zu bytes): %s\n", bytes_read, read_buf);
        
//         file_close(file);
//     }

//     cache_print_stats();

//     // Cleanup
//     vfs_shutdown();

//     printf("\n=== Demo Complete ===\n");
//     return 0;
// }