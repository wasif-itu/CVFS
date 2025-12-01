

/* ================================================================
 * FILE: core/inode.c
 * ================================================================ */
#include "inode.h"
#include "vfs.h"
#include <stdlib.h>
#include <string.h>

Inode *inode_read(uint64_t inode_number) {
    // Read inode from block (simplified)
    size_t size;
    uint8_t *data = vfs_read_block(inode_number, &size);
    
    if (data == NULL) {
        return NULL;
    }

    Inode *inode = (Inode *)malloc(sizeof(Inode));
    if (inode != NULL && size >= sizeof(Inode)) {
        memcpy(inode, data, sizeof(Inode));
    }

    free(data);
    return inode;
}

void inode_write(Inode *inode) {
    if (inode == NULL) {
        return;
    }

    vfs_write_block(inode->inode_number, (uint8_t *)inode, sizeof(Inode));
}

void inode_free(Inode *inode) {
    free(inode);
}