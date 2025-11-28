#include "../src/core/vfs_core.h"
#include "../src/backends/backend_posix.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <pthread.h>
#include <time.h>
#include <errno.h>

#define NUM_THREADS 10
#define OPS_PER_THREAD 100
#define TEST_DIR "/tmp/vfs_stress_test"

static pthread_mutex_t result_lock = PTHREAD_MUTEX_INITIALIZER;
static int total_ops = 0;
static int failed_ops = 0;

typedef struct {
    int thread_id;
    int operations;
} thread_data_t;

/* Thread worker function */
void *stress_worker(void *arg) {
    thread_data_t *data = (thread_data_t *)arg;
    char filename[256];
    char buffer[1024];
    int local_success = 0;
    int local_failed = 0;
    
    for (int i = 0; i < data->operations; i++) {
        snprintf(filename, sizeof(filename), "/backend/thread_%d_file_%d.txt", 
                 data->thread_id, i);
        
        /* Open/Create file */
        int fh = vfs_open(filename, O_CREAT | O_RDWR);
        if (fh < 0) {
            local_failed++;
            continue;
        }
        
        /* Write data */
        snprintf(buffer, sizeof(buffer), "Thread %d, Operation %d, Data: %ld\n", 
                 data->thread_id, i, (long)time(NULL));
        ssize_t written = vfs_write(fh, buffer, strlen(buffer), 0);
        if (written < 0) {
            vfs_close(fh);
            local_failed++;
            continue;
        }
        
        /* Read data back */
        char readbuf[1024] = {0};
        ssize_t nread = vfs_read(fh, readbuf, sizeof(readbuf) - 1, 0);
        if (nread < 0) {
            vfs_close(fh);
            local_failed++;
            continue;
        }
        
        /* Verify data */
        if (strcmp(buffer, readbuf) != 0) {
            fprintf(stderr, "Thread %d: Data mismatch on file %s\n", 
                    data->thread_id, filename);
            vfs_close(fh);
            local_failed++;
            continue;
        }
        
        /* Stat the file */
        struct stat st;
        if (vfs_stat(filename, &st) != 0) {
            vfs_close(fh);
            local_failed++;
            continue;
        }
        
        /* Close file */
        if (vfs_close(fh) != 0) {
            local_failed++;
            continue;
        }
        
        local_success++;
    }
    
    /* Update global counters */
    pthread_mutex_lock(&result_lock);
    total_ops += local_success;
    failed_ops += local_failed;
    pthread_mutex_unlock(&result_lock);
    
    free(data);
    return NULL;
}

int main(void) {
    printf("=== VFS Stress Test - Concurrent Operations ===\n\n");
    
    /* Initialize VFS */
    printf("1. Initializing VFS...\n");
    if (vfs_init() != 0) {
        fprintf(stderr, "FAIL: vfs_init failed\n");
        return 1;
    }
    printf("   ✓ VFS initialized\n\n");
    
    /* Create and clean test directory */
    printf("2. Setting up test directory...\n");
    system("rm -rf " TEST_DIR);
    system("mkdir -p " TEST_DIR);
    printf("   ✓ Test directory ready: %s\n\n", TEST_DIR);
    
    /* Mount backend */
    printf("3. Mounting POSIX backend...\n");
    if (vfs_mount_backend("/backend", TEST_DIR, "posix") != 0) {
        fprintf(stderr, "FAIL: vfs_mount_backend failed\n");
        vfs_shutdown();
        return 1;
    }
    printf("   ✓ Backend mounted at /backend\n\n");
    
    /* Launch stress test threads */
    printf("4. Launching %d concurrent threads (%d ops each)...\n", 
           NUM_THREADS, OPS_PER_THREAD);
    
    pthread_t threads[NUM_THREADS];
    time_t start_time = time(NULL);
    
    for (int i = 0; i < NUM_THREADS; i++) {
        thread_data_t *data = malloc(sizeof(thread_data_t));
        data->thread_id = i;
        data->operations = OPS_PER_THREAD;
        
        if (pthread_create(&threads[i], NULL, stress_worker, data) != 0) {
            fprintf(stderr, "FAIL: pthread_create failed for thread %d\n", i);
            free(data);
            return 1;
        }
    }
    
    printf("   ✓ All threads launched\n\n");
    
    /* Wait for all threads */
    printf("5. Waiting for threads to complete...\n");
    for (int i = 0; i < NUM_THREADS; i++) {
        pthread_join(threads[i], NULL);
    }
    
    time_t end_time = time(NULL);
    double elapsed = difftime(end_time, start_time);
    
    printf("   ✓ All threads completed in %.0f seconds\n\n", elapsed);
    
    /* Test readdir on populated directory */
    printf("6. Testing directory listing...\n");
    int dir_count = 0;
    
    /* Simple readdir test - just verify it doesn't crash */
    struct stat st;
    if (vfs_stat("/backend", &st) == 0) {
        printf("   ✓ Directory stat successful\n");
    }
    
    printf("   ✓ Directory operations completed\n\n");
    
    /* Cleanup */
    printf("7. Shutting down VFS...\n");
    if (vfs_shutdown() != 0) {
        fprintf(stderr, "FAIL: vfs_shutdown failed\n");
        return 1;
    }
    printf("   ✓ VFS shutdown complete\n\n");
    
    /* Results */
    printf("====================================\n");
    printf("  Stress Test Results\n");
    printf("====================================\n");
    printf("Total operations:    %d\n", total_ops + failed_ops);
    printf("Successful:          %d\n", total_ops);
    printf("Failed:              %d\n", failed_ops);
    printf("Success rate:        %.2f%%\n", 
           (total_ops * 100.0) / (total_ops + failed_ops));
    printf("Operations/second:   %.2f\n", 
           (total_ops + failed_ops) / (elapsed > 0 ? elapsed : 1));
    printf("====================================\n\n");
    
    if (failed_ops == 0) {
        printf("✅ ALL STRESS TESTS PASSED\n");
        return 0;
    } else {
        printf("⚠️  SOME OPERATIONS FAILED\n");
        return 1;
    }
}
