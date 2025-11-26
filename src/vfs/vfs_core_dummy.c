#include "../fuse/vfs_fuse.h"
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <stdio.h>

/* -------------- Simple in-memory file table -------------- */

struct file_entry {
    char *path;
    char *data;
    size_t size;
};

static struct file_entry files[128];
static int file_count = 0;

static struct file_entry *find_file(const char *path)
{
    for (int i = 0; i < file_count; i++)
        if (strcmp(files[i].path, path) == 0)
            return &files[i];
    return NULL;
}

static struct file_entry *create_file(const char *path)
{
    if (file_count >= 128) return NULL;
    files[file_count].path = strdup(path);
    files[file_count].data = calloc(1, 1);
    files[file_count].size = 0;
    return &files[file_count++];
}

/* ---------------- VFS API IMPLEMENTATION ---------------- */

int vfs_getattr(const char *path, struct stat *st)
{
    memset(st, 0, sizeof(*st));

    if (strcmp(path, "/") == 0) {
        st->st_mode = S_IFDIR | 0755;
        st->st_nlink = 2;
        return 0;
    }

    struct file_entry *f = find_file(path);
    if (!f) return -ENOENT;

    st->st_mode = S_IFREG | 0644;
    st->st_nlink = 1;
    st->st_size = f->size;
    return 0;
}

ssize_t vfs_read(const char *path, char *buf, size_t size, off_t offset)
{
    struct file_entry *f = find_file(path);
    if (!f) return -ENOENT;

    if (offset >= f->size) return 0;

    if (offset + size > f->size)
        size = f->size - offset;

    memcpy(buf, f->data + offset, size);
    return size;
}

ssize_t vfs_write(const char *path, const char *buf, size_t size, off_t offset)
{
    struct file_entry *f = find_file(path);
    if (!f) f = create_file(path);

    if (offset + size > f->size) {
        f->data = realloc(f->data, offset + size);
        f->size = offset + size;
    }

    memcpy(f->data + offset, buf, size);
    return size;
}

int vfs_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
                off_t offset, struct fuse_file_info *fi)
{
    (void)offset; (void)fi;

    if (strcmp(path, "/") != 0)
        return -ENOENT;

    filler(buf, ".", NULL, 0, 0);
    filler(buf, "..", NULL, 0, 0);

    for (int i = 0; i < file_count; i++) {
        const char *name = files[i].path + 1; // skip '/'
        filler(buf, name, NULL, 0, 0);
    }

    return 0;
}

int vfs_mkdir(const char *path, mode_t mode)
{
    return 0;   /* do nothing */
}

int vfs_mknod(const char *path, mode_t mode, dev_t rdev)
{
    create_file(path);
    return 0;
}

int vfs_open(const char *path, struct fuse_file_info *fi)
{
    if (!find_file(path)) return -ENOENT;
    return 0;
}

int vfs_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
    create_file(path);
    return 0;
}

ssize_t vfs_readlink(const char *path, char *buf, size_t size)
{
    snprintf(buf, size, "/dummy/target");
    return strlen(buf);
}

int vfs_symlink(const char *t, const char *l)
{
    return 0; /* pretend success */
}
