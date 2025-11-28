#ifndef BACKEND_POSIX_H
#define BACKEND_POSIX_H

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

/* When building with libfuse3, this type matches fuse_fill_dir_t.
 * We include fuse3 headers in the C file, but keep the header minimal.
 */
typedef int (*vfs_fill_dir_t)(void *buf, const char *name, const struct stat *st, off_t off);

/* Initialize a posix backend for the given root path.
 * Returns backend_id >= 1 on success, or -1 on error (errno set).
 */
int posix_backend_init(const char *rootpath);

/* Shutdown/destroy backend */
int posix_backend_shutdown(int backend_id);

/* Open a file in the backend. Returns a backend-specific handle (>0) or -1 on error (errno set).
 * Flags and mode are POSIX-style flags and mode.
 */
int posix_open(int backend_id, const char *relpath, int flags, mode_t mode);

/* Close a handle returned by posix_open */
int posix_close(int backend_id, int handle);

/* Read/write using handle (pread/pwrite semantics) */
ssize_t posix_read(int backend_id, int handle, void *buf, size_t count, off_t offset);
ssize_t posix_write(int backend_id, int handle, const void *buf, size_t count, off_t offset);

/* Stat a relative path within backend, filling struct stat */
int posix_stat(int backend_id, const char *relpath, struct stat *st);

/* Read directory entries. Uses a filler compatible with fuse_fill_dir_t semantics.
 * Returns 0 on success, -1 on error (errno set).
 */
int posix_readdir(int backend_id, const char *relpath, void *buf, vfs_fill_dir_t filler, off_t offset);


/* Create file (like open with O_CREAT | O_EXCL). Returns handle (>0) or -1 */
int posix_create(int backend_id, const char *relpath, mode_t mode);

/* Unlink (delete) a file. Returns 0 on success, -1 on error */
int posix_unlink(int backend_id, const char *relpath);

/* Rename a file within the same backend (old -> new). Returns 0 on success */
int posix_rename(int backend_id, const char *old_relpath, const char *new_relpath);

/* Make directory. Returns 0 on success */
int posix_mkdir(int backend_id, const char *relpath, mode_t mode);

/* Forward declaration for VFS integration */
struct vfs_backend_ops;

/* Get the VFS backend ops structure for POSIX backend */
const struct vfs_backend_ops *get_posix_backend_ops(void);

#endif /* BACKEND_POSIX_H */
