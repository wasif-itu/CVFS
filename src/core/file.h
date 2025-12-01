

#ifndef FILE_H
#define FILE_H

#include <stdint.h>
#include <stddef.h>
#include "inode.h"

typedef struct File {
    Inode *inode;
    uint64_t position;
} File;

File *file_open(uint64_t inode_number);
size_t file_read(File *file, uint8_t *buffer, size_t count);
size_t file_write(File *file, const uint8_t *buffer, size_t count);
void file_close(File *file);

#endif // FILE_H

