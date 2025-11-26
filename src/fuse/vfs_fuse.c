/* vfs_fuse.c
 *
 * FUSE3 skeleton that forwards calls to the VFS API (stubs declared in vfs_fuse.h).
 * This file intentionally contains minimal business logic and acts as the glue
 * between libfuse3 and your VFS implementation.
 */

#define FUSE_USE_VERSION 31

#include "vfs_fuse.h"       /* must declare vfs_* stubs and my_fuse_* prototypes */
#include <fuse3/fuse.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>

/* Define the operations structure (as requested in your architecture).
 * The function pointers must match the libfuse3 signatures.
 */
struct fuse_operations my_fuse_ops = {
    .getattr  = my_fuse_getattr,
    .read     = my_fuse_read,
    .write    = my_fuse_write,
    .readdir  = my_fuse_readdir,
    .mkdir    = my_fuse_mkdir,
    .mknod    = my_fuse_mknod,
    .open     = my_fuse_open,
    .create   = my_fuse_create,
    .readlink = my_fuse_readlink,
    .symlink  = my_fuse_symlink,
};

/* Helper: convert a negative vfs return value to FUSE-style negative errno.
 * If vfs_* returns negative errno (e.g. -ENOENT) return that directly.
 * If it returns other negative, return -EIO as a conservative default.
 *
 * Note: only call this when `v` is negative.
 */
static int vfs_to_fuse_err(int v)
{
    if (v >= 0)
        return v;
    int e = -v;
    if (e > 0 && e < 4096) /* crude errno range check */
        return -e;
    return -EIO;
}

/* ---------- FUSE callback wrappers (match FUSE3 signatures) ---------- */

/* getattr: note the extra `struct fuse_file_info *fi` parameter in FUSE3 */
int my_fuse_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{
    (void)fi;
    if (stbuf)
        memset(stbuf, 0, sizeof(*stbuf));
    int r = vfs_getattr(path, stbuf);
    if (r == 0)
        return 0;
    return vfs_to_fuse_err(r);
}

/* read */
int my_fuse_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    (void)fi;
    ssize_t r = vfs_read(path, buf, size, offset);
    if (r >= 0)
        return (int)r; /* FUSE expects non-negative number of bytes */
    return vfs_to_fuse_err((int)r);
}

/* write */
int my_fuse_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    (void)fi;
    ssize_t r = vfs_write(path, buf, size, offset);
    if (r >= 0)
        return (int)r;
    return vfs_to_fuse_err((int)r);
}

/* readdir -- FUSE3 adds the enum fuse_readdir_flags parameter at the end.
 * We pass through to the VFS layer which is expected to call the provided
 * filler callback. We ignore flags for now (forwarding is a reasonable default).
 */
int my_fuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                    off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags)
{
    (void)fi;
    (void)flags;
    int r = vfs_readdir(path, buf, filler, offset, fi);
    if (r == 0)
        return 0;
    return vfs_to_fuse_err(r);
}

/* mkdir */
int my_fuse_mkdir(const char *path, mode_t mode)
{
    int r = vfs_mkdir(path, mode);
    if (r == 0)
        return 0;
    return vfs_to_fuse_err(r);
}

/* mknod */
int my_fuse_mknod(const char *path, mode_t mode, dev_t rdev)
{
    int r = vfs_mknod(path, mode, rdev);
    if (r == 0)
        return 0;
    return vfs_to_fuse_err(r);
}

/* open */
int my_fuse_open(const char *path, struct fuse_file_info *fi)
{
    int r = vfs_open(path, fi);
    if (r == 0)
        return 0;
    return vfs_to_fuse_err(r);
}

/* create */
int my_fuse_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    int r = vfs_create(path, mode, fi);
    if (r == 0)
        return 0;
    return vfs_to_fuse_err(r);
}

/* readlink
 * Our VFS readlink returns ssize_t: number of bytes written or negative errno.
 * FUSE expects 0 on success (the link target is placed into buf), or -errno.
 */
int my_fuse_readlink(const char *path, char *buf, size_t size)
{
    ssize_t r = vfs_readlink(path, buf, size);
    if (r >= 0) {
        /* ensure null termination */
        if ((size_t)r < size)
            buf[r] = '\0';
        else if (size > 0)
            buf[size - 1] = '\0';
        return 0;
    }
    return vfs_to_fuse_err((int)r);
}

/* symlink */
int my_fuse_symlink(const char *target, const char *linkpath)
{
    int r = vfs_symlink(target, linkpath);
    if (r == 0)
        return 0;
    return vfs_to_fuse_err(r);
}

/* ---------- main helper ---------- */

int main(int argc, char *argv[])
{
    /* Optional: initialize VFS here if you have an init function */
    fprintf(stderr, "Starting FUSE filesystem (vfs_fuse glue)\n");
    return fuse_main(argc, argv, &my_fuse_ops, NULL);
}
