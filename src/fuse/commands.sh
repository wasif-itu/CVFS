# BUILD

# Try fuse3:
gcc -Wall $(pkg-config --cflags fuse3) vfs_fuse.c vfs_core_dummy.c -o vfs_fuse $(pkg-config --libs fuse3)

# If that fails, try fuse:
gcc -Wall $(pkg-config --cflags fuse) vfs_fuse.c vfs_core_dummy.c -o vfs_fuse $(pkg-config --libs fuse)

# MOUNT
mkdir -p /tmp/fuse-test-mount

# VFS
./vfs_fuse -f /tmp/fuse-test-mount

# FUSE

ls -la /tmp/fuse-test-mount

mkdir /tmp/fuse-test-mount/dir1
ls -la /tmp/fuse-test-mount

touch /tmp/fuse-test-mount/dir1/file1
ls -la /tmp/fuse-test-mount/dir1

echo "hello world" > /tmp/fuse-test-mount/dir1/file1
cat /tmp/fuse-test-mount/dir1/file1

dd if=/tmp/fuse-test-mount/dir1/file1 bs=1 skip=6 count=5 status=none

# Attempt to read beyond file size (should return empty)
tail -c +100 /tmp/fuse-test-mount/dir1/file1 || true


ln -s /tmp/fuse-test-mount/dir1/file1 /tmp/fuse-test-mount/link_to_file1
readlink /tmp/fuse-test-mount/link_to_file1
cat /tmp/fuse-test-mount/link_to_file1

ls -R /tmp/fuse-test-mount

mkdir /tmp/fuse-test-mount/dir1/subdir
printf "abc" > /tmp/fuse-test-mount/dir1/subdir/f2
cat /tmp/fuse-test-mount/dir1/subdir/f2
