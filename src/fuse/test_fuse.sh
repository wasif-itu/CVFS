#!/bin/bash
set -e
M=/tmp/fuse-test-mount

echo "Listing root:"
ls -la $M

echo "mkdir dirA"
mkdir -p $M/dirA

echo "create file"
echo filecontent > $M/dirA/foo.txt

echo "read file"
cat $M/dirA/foo.txt

echo "symlink"
ln -s $M/dirA/foo.txt $M/foo-link
readlink $M/foo-link
cat $M/foo-link

echo "readdir"
ls -R $M

echo "done"

# fusermount -u /tmp/fuse-test-mount    # Linux