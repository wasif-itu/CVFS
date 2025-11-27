#ifndef VFS_FUSE_H
#define VFS_FUSE_H

#define FUSE_USE_VERSION 31

#include <fuse3/fuse.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C"
{
#endif

  /* FUSE callback wrappers (implementations in vfs_fuse.c) */
  int my_fuse_getattr(const char *path, struct stat *st, struct fuse_file_info *fi);
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
  void *my_fuse_init(struct fuse_conn_info *conn, struct fuse_config *cfg);
  void my_fuse_destroy(void *private_data);
  int my_fuse_unlink(const char *path);
  int my_fuse_rename(const char *oldpath, const char *newpath, unsigned int flags);
  /* The operations table */
  extern struct fuse_operations my_fuse_ops;

  /*
   * These are the VFS core functions that this glue expects to exist.
   * Your vfs_core.h as posted declares these; include that header in the
   * compilation unit that links with this file.
   *
   * Expected prototypes (from your vfs_core.h):
   *
   * int vfs_init(void);
   * int vfs_shutdown(void);
   *
   * int vfs_stat(const char *path, struct stat *st);
   *
   * int vfs_open(const char *path, int flags);
   * ssize_t vfs_read(int fh, void *buf, size_t count, off_t offset);
   * ssize_t vfs_write(int fh, const void *buf, size_t count, off_t offset);
   *
   * int vfs_readdir(const char *path, void *buf, void *filler);
   *
   * (If you add more vfs_ functions such as mkdir/mknod/create/symlink/readlink,
   *  update the glue to forward to them instead of returning -ENOSYS.)
   */

#ifdef __cplusplus
}
#endif

#endif /* VFS_FUSE_H */
