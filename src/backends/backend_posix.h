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

#endif /* BACKEND_POSIX_H */
