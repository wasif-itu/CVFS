
/* ================================================================
 * FILE: core/file.c
 * ================================================================ */
#include "file.h"
#include "vfs.h"
#include <stdlib.h>
#include <string.h>

File *file_open(uint64_t inode_number) {
    Inode *inode = inode_read(inode_number);
    if (inode == NULL) {
        return NULL;
    }

    File *file = (File *)malloc(sizeof(File));
    if (file != NULL) {
        file->inode = inode;
        file->position = 0;
    }

    return file;
}

size_t file_read(File *file, uint8_t *buffer, size_t count) {
    if (file == NULL || buffer == NULL) {
        return 0;
    }

    // Simplified: read from first data block
    if (file->inode->block_ids[0] == 0) {
        return 0;
    }

    size_t size;
    uint8_t *data = vfs_read_block(file->inode->block_ids[0], &size);
    
    if (data == NULL) {
        return 0;
    }

    size_t to_read = (count < size) ? count : size;
    memcpy(buffer, data, to_read);
    free(data);

    return to_read;
}

size_t file_write(File *file, const uint8_t *buffer, size_t count) {
    if (file == NULL || buffer == NULL) {
        return 0;
    }

    // Simplified: write to first data block
    uint64_t block_id = file->inode->block_ids[0];
    if (block_id == 0) {
        block_id = file->inode->inode_number + 1;
        file->inode->block_ids[0] = block_id;
    }

    vfs_write_block(block_id, (uint8_t *)buffer, count);
    file->inode->size = count;
    inode_write(file->inode);

    return count;
}

void file_close(File *file) {
    if (file != NULL) {
        inode_free(file->inode);
        free(file);
    }
}
