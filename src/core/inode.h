
/* ================================================================
 * FILE: core/inode.h
 * ================================================================ */
#ifndef INODE_H
#define INODE_H

#include <stdint.h>
#include <stddef.h>

typedef struct Inode {
    uint64_t inode_number;
    uint64_t size;
    uint64_t block_ids[16];  // Direct block pointers
    uint32_t mode;
    uint32_t uid;
    uint32_t gid;
} Inode;

Inode *inode_read(uint64_t inode_number);
void inode_write(Inode *inode);
void inode_free(Inode *inode);

#endif // INODE_H
