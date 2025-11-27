/* vfs_fuse.c
 *
 * FUSE3 glue that forwards calls to the VFS core (vfs_core.h).
 *
 * This implementation expects the vfs_core API you posted earlier:
 *   - vfs_init(), vfs_shutdown()
 *   - vfs_stat(path, struct stat *)
 *   - vfs_open(path, flags) -> returns file-handle (int) or negative errno
 *   - vfs_read(fh, buf, count, offset)
 *   - vfs_write(fh, buf, count, offset)
 *   - vfs_readdir(path, buf, filler)
 *
 * For missing operations (mkdir/mknod/create/symlink/readlink) it returns -ENOSYS.
 */

#define FUSE_USE_VERSION 31

/* ---------- Control directory ---------- */
#define CTRL_DIR "/vfs_control"
#define CTRL_LIST "/vfs_control/snapshot_list"
#define CTRL_CREATE "/vfs_control/snapshot_create"
#define CTRL_RESTORE "/vfs_control/snapshot_restore"

#include "vfs_fuse.h"
#include "../core/vfs_core.h"
#include <fuse3/fuse.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <stdint.h>

/* Helper: convert a negative vfs return value to FUSE-style negative errno.
 * If vfs_* returns negative errno (e.g. -ENOENT) return that directly.
 * If it returns other negative, return -EIO as a conservative default.
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

/* ---------- FUSE callbacks ---------- */

void *my_fuse_init(struct fuse_conn_info *conn, struct fuse_config *cfg)
{
    (void)conn;
    (void)cfg;
    fprintf(stderr, "[vfs_fuse] init\n");
    int r = vfs_init();
    if (r != 0)
    {
        fprintf(stderr, "[vfs_fuse] vfs_init failed: %d\n", r);
        /* We return NULL even on failure — FUSE will continue but ops may fail. */
    }
    return NULL;
}

void my_fuse_destroy(void *private_data)
{
    (void)private_data;
    fprintf(stderr, "[vfs_fuse] destroy\n");
    int r = vfs_shutdown();
    if (r != 0)
    {
        fprintf(stderr, "[vfs_fuse] vfs_shutdown returned %d\n", r);
    }
}

int my_fuse_getattr(const char *path, struct stat *stbuf, struct fuse_file_info *fi)
{
    (void)fi;
    if (stbuf)
        memset(stbuf, 0, sizeof(*stbuf));

    /* Virtual control directory */
    if (strcmp(path, CTRL_DIR) == 0)
    {
        stbuf->st_mode = S_IFDIR | 0555;
        stbuf->st_nlink = 2;
        return 0;
    }
    if (strcmp(path, CTRL_LIST) == 0 ||
        strcmp(path, CTRL_CREATE) == 0 ||
        strcmp(path, CTRL_RESTORE) == 0)
    {
        stbuf->st_mode = S_IFREG | 0666;
        stbuf->st_nlink = 1;
        stbuf->st_size = 0;
        return 0;
    }

    return vfs_to_fuse_err(vfs_getattr(path, stbuf));
}

/* FUSE open: call vfs_open(path, fi->flags). On success store the returned
 * integer file-handle in fi->fh (use uint64 cast). vfs_open is expected to
 * return a non-negative file handle or negative errno.
 */
int my_fuse_open(const char *path, struct fuse_file_info *fi)
{
    /* Get caller identity */
    struct fuse_context *ctx = fuse_get_context();
    uid_t uid = ctx->uid;
    gid_t gid = ctx->gid;

    /* Determine permission mask from open flags */
    int flags = fi ? fi->flags : O_RDONLY;
    int mask = 0;

    if ((flags & O_ACCMODE) == O_RDONLY)
        mask = R_OK;
    else if ((flags & O_ACCMODE) == O_WRONLY)
        mask = W_OK;
    else if ((flags & O_ACCMODE) == O_RDWR)
        mask = R_OK | W_OK;

    /* Call internal permission layer */
    int perm = vfs_permission_check(path, uid, gid, mask);
    if (perm < 0)
        return -EACCES; // Deny access

    /* Proceed with actual open */
    int fh = vfs_open(path, flags);
    if (fh < 0)
        return vfs_to_fuse_err(fh);

    /* Save file handle */
    if (fi)
        fi->fh = (uint64_t)(uintptr_t)fh;

    return 0;
}

/* read: use fi->fh as file-handle. If fi->fh is 0, attempt to open as fallback.
 * Returns number of bytes read (non-negative) or negative errno.
 */
int my_fuse_read(const char *path, char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    (void)fi;

    /* Virtual snapshot_list */
    if (strcmp(path, CTRL_LIST) == 0)
    {
        char tmp[8192];
        int r = vfs_snapshot_list(tmp, sizeof(tmp));
        if (r < 0)
            return vfs_to_fuse_err(r);
        size_t len = strlen(tmp);
        if ((size_t)offset >= len)
            return 0;
        if (offset + size > len)
            size = len - offset;
        memcpy(buf, tmp + offset, size);
        return size;
    }

    return vfs_read(path, buf, size, offset);
}

/* write: similar pattern to read */
int my_fuse_write(const char *path, const char *buf, size_t size, off_t offset, struct fuse_file_info *fi)
{
    (void)offset;
    (void)fi;

    /* Virtual snapshot_create */
    if (strcmp(path, CTRL_CREATE) == 0)
    {
        char tmp[4096];
        if (size >= sizeof(tmp))
            return -EOVERFLOW;
        memcpy(tmp, buf, size);
        tmp[size] = 0;

        char snapname[256];
        int r = vfs_snapshot_create(tmp, snapname, sizeof(snapname));
        if (r < 0)
            return vfs_to_fuse_err(r);
        fprintf(stderr, "[snapshot created] %s -> %s\n", tmp, snapname);
        return size;
    }

    /* Virtual snapshot_restore */
    if (strcmp(path, CTRL_RESTORE) == 0)
    {
        char tmp[4096];
        if (size >= sizeof(tmp))
            return -EOVERFLOW;
        memcpy(tmp, buf, size);
        tmp[size] = 0;

        char *snap = strtok(tmp, " \t\n");
        char *tgt = strtok(NULL, " \t\n");
        if (!snap || !tgt)
            return -EINVAL;

        int r = vfs_snapshot_restore(snap, tgt);
        if (r < 0)
            return vfs_to_fuse_err(r);
        fprintf(stderr, "[snapshot restored] %s -> %s\n", snap, tgt);
        return size;
    }

    return vfs_write(path, buf, size, offset);
}

/* readdir: forward to vfs_readdir. The VFS readdir signature in your header
 * was: int vfs_readdir(const char *path, void *buf, void *filler);
 * FUSE provides a fuse_fill_dir_t filler; pass it through.
 *
 * We ignore offset/fi/flags and let the VFS handle the filler callback.
 */
int my_fuse_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                    off_t offset, struct fuse_file_info *fi, enum fuse_readdir_flags flags)
{
    (void)offset;
    (void)fi;
    (void)flags;

    /* Virtual control directory */
    if (strcmp(path, CTRL_DIR) == 0)
    {
        filler(buf, ".", NULL, 0, 0);
        filler(buf, "..", NULL, 0, 0);
        filler(buf, "snapshot_list", NULL, 0, 0);
        filler(buf, "snapshot_create", NULL, 0, 0);
        filler(buf, "snapshot_restore", NULL, 0, 0);
        return 0;
    }

    return vfs_readdir(path, buf, filler, offset, fi);
}

/* mkdir/mknod/create/symlink/readlink: not implemented in vfs_core.h you provided.
 * Return ENOSYS so the caller sees "function not implemented". Replace these with
 * calls into your VFS when you add the corresponding vfs_* APIs.
 */

int my_fuse_mknod(const char *path, mode_t mode, dev_t rdev)
{
    int r = vfs_mknod(path, mode, rdev);
    if (r == 0)
        return 0;
    return vfs_to_fuse_err(r);
}

/* mkdir -> vfs_mkdir */
int my_fuse_mkdir(const char *path, mode_t mode)
{
    int r = vfs_mkdir(path, mode);
    if (r == 0)
        return 0;
    return vfs_to_fuse_err(r);
}

/* unlink -> vfs_unlink */
int my_fuse_unlink(const char *path)
{
    struct fuse_context *ctx = fuse_get_context();
    if (vfs_permission_check(path, ctx->uid, ctx->gid, W_OK) < 0)
        return -EACCES;

    int r = vfs_unlink(path);
    if (r < 0) return vfs_to_fuse_err(r);
    return 0;
}

/* rename -> vfs_rename
 * FUSE3 has a flags parameter; ignore it for now (or pass through if you extend vfs_rename)
 */
int my_fuse_rename(const char *oldpath, const char *newpath, unsigned int flags)
{
    (void)flags;
    struct fuse_context *ctx = fuse_get_context();
    if (vfs_permission_check(oldpath, ctx->uid, ctx->gid, W_OK) < 0)
        return -EACCES;

    int r = vfs_rename(oldpath, newpath);
    if (r < 0) return vfs_to_fuse_err(r);
    return 0;
}

/* create -> vfs_create
 *
 * vfs_create may follow one of two common conventions:
 * - return 0 and populate fi->fh inside the vfs
 * - return a non-negative file-handle integer (>=0)
 *
 * We handle both:
 *  - if r < 0 : convert to errno
 *  - if r == 0 : assume fi->fh already set by vfs_create
 *  - if r > 0 : treat r as file-handle and store it in fi->fh
 */
int my_fuse_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    if (!fi)
        return -EINVAL;

    struct fuse_context *ctx = fuse_get_context();
    if (vfs_permission_check(path, ctx->uid, ctx->gid, W_OK) < 0)
        return -EACCES;

    int r = vfs_create(path, mode, fi);
    if (r < 0) return vfs_to_fuse_err(r);
    if (r > 0) fi->fh = (uint64_t)(uintptr_t) r;
    return 0;
}

/* readlink */
int my_fuse_readlink(const char *path, char *buf, size_t size)
{
    ssize_t r = vfs_readlink(path, buf, size);
    if (r >= 0)
    {
        if ((size_t)r < size) buf[r] = '\0';
        else if (size > 0) buf[size-1] = '\0';
        return 0;
    }
    return vfs_to_fuse_err((int)r);
}

/* symlink */
int my_fuse_symlink(const char *target, const char *linkpath)
{
    int r = vfs_symlink(target, linkpath);
    if (r < 0) return vfs_to_fuse_err(r);
    return 0;
}

/* FUSE operations table */
struct fuse_operations my_fuse_ops = {
    .getattr = my_fuse_getattr,
    .read = my_fuse_read,
    .write = my_fuse_write,
    .readdir = my_fuse_readdir,
    .mkdir = my_fuse_mkdir,
    .mknod = my_fuse_mknod,
    .open = my_fuse_open,
    .create = my_fuse_create,
    .readlink = (int (*)(const char *, char *, size_t))my_fuse_readlink,
    .symlink = my_fuse_symlink,
    .init = my_fuse_init,
    .destroy = my_fuse_destroy,
};

int main(int argc, char *argv[])
{
    fprintf(stderr, "Starting FUSE filesystem (vfs_fuse glue)\n");
    return fuse_main(argc, argv, &my_fuse_ops, NULL);
}
