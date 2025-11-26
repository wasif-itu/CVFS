#ifndef VFS_FUSE_H
#define VFS_FUSE_H

#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <sys/types.h>
#include <sys/stat.h>

#ifdef __cplusplus
extern "C" {
#endif

/* -------- FUSE callback wrappers ---------- */
int my_fuse_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi);
int my_fuse_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int my_fuse_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi);
int my_fuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                    off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags);
int my_fuse_mkdir(const char *path, mode_t mode);
int my_fuse_mknod(const char *path, mode_t mode, dev_t rdev);
int my_fuse_open(const char *path, struct fuse_file_info *fi);
int my_fuse_create(const char *path, mode_t mode, struct fuse_file_info *fi);
int my_fuse_readlink(const char *path, char *buf, size_t size);
int my_fuse_symlink(const char *target, const char *linkpath);

/* The operations table */
extern struct fuse_operations my_fuse_ops;

/* ========= Minimal VFS API (dummy implementation in vfs_core_dummy.c) ======== */

/* Metadata */
int vfs_getattr(const char *path, struct stat *stbuf);

/* File I/O */
ssize_t vfs_read(const char *path, char *buf, size_t size, off_t offset);
ssize_t vfs_write(const char *path, const char *buf, size_t size, off_t offset);

/* Directory read */
int vfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                off_t offset, struct fuse_file_info *fi);

/* Node creation */
int vfs_mkdir(const char *path, mode_t mode);
int vfs_mknod(const char *path, mode_t mode, dev_t rdev);

/* Open/create */
int vfs_open(const char *path, struct fuse_file_info *fi);
int vfs_create(const char *path, mode_t mode, struct fuse_file_info *fi);

/* Symlinks */
ssize_t vfs_readlink(const char *path, char *buf, size_t size);
int vfs_symlink(const char *target, const char *linkpath);

#ifdef __cplusplus
}
#endif

#endif
