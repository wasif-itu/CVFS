# CVFS — Custom Virtual File System

CVFS is a teaching-oriented, production-quality Virtual File System (VFS) core with a POSIX backend and a FUSE3 userspace filesystem layer. It demonstrates clean reference-counted core structures (inodes, dentries, file handles), robust backend dispatch, and a thin FUSE glue to expose the VFS through the Linux kernel interface under WSL.

## Highlights
- Clean VFS Core with clear APIs (`vfs_init`, `vfs_open`, `vfs_read`, `vfs_write`, `vfs_close`, `vfs_stat`, `vfs_readdir`, `vfs_mkdir`, etc.)
- POSIX backend implementation for real file operations on disk
- FUSE3 integration to mount and interact with the filesystem via standard shell commands
- Comprehensive test suite (unit, integration, stress)
- Valgrind-clean memory management (0 bytes leaked across all tests)
- Thread-safe concurrent operations validated (1000 ops, 100% success)

## Repository Layout
```
CVFS/
  Makefile
  build.sh                # Unified helper: build/test/fuse lifecycle
  src/
    core/                 # VFS core (APIs, refcounting, path resolution)
    backends/             # POSIX backend
    fuse/                 # FUSE3 glue layer
    tools/                # CLI tool(s)
  tests/                  # Unit, integration, stress, FUSE test scripts
  README.md               # This file
  TEST_RESULTS.md         # Detailed test report (generated)
```

## Prerequisites (WSL Ubuntu)
Install once inside WSL:
```bash
sudo apt update
sudo apt install -y build-essential pkg-config fuse3 libfuse3-dev sqlite3 libsqlite3-dev dos2unix
```

Ensure FUSE is enabled in WSL. Most modern WSL distributions support FUSE out of the box. If unmount issues occur, ensure no shell is currently inside the mount directory.

## Quick Start
From Windows PowerShell:
```powershell
cd 'c:\Users\inbox\Desktop\Semester 5\OS\CVFS'
wsl bash -c "./build.sh build"
wsl bash -c "./build.sh fuse:start"
wsl bash -c "./build.sh fuse:status"
wsl bash -c "./build.sh fuse:stop"
```

Or use the one-shot demo:
```powershell
wsl bash -c "./build.sh demo"
```

## Interactive Usage (Recommended)
Drop into a shell rooted at the mountpoint and play with the filesystem like you normally would:
```powershell
wsl bash -c "./build.sh fuse:shell"
```

Inside that shell (Linux bash):
```bash
# Explore
pwd
ls -la

# Create directories and files
mkdir -p projects/demo
echo "Hello CVFS!" > projects/demo/hello.txt
cat projects/demo/hello.txt

# Copy/move/rename
cp projects/demo/hello.txt projects/demo/hello_copy.txt
mv projects/demo/hello_copy.txt projects/demo/renamed.txt
ls -la projects/demo

# Inspect metadata
stat .
stat projects/demo
stat projects/demo/hello.txt

# Edit
printf "More data\n" >> projects/demo/hello.txt
sed -n '1,5p' projects/demo/hello.txt

# Remove
rm projects/demo/renamed.txt
rm -rf projects/demo

# Exit the shell when done, then unmount:
exit
```
From PowerShell:
```powershell
wsl bash -c "./build.sh fuse:stop"
```

## Unified Helper — `build.sh`
The project ships with a unified helper script that wraps common tasks:

```bash
./build.sh build        # clean build via Makefile
./build.sh test         # run unit + integration + stress tests
./build.sh valgrind     # run valgrind leak checks
./build.sh fuse:start   # mount FUSE FS at /tmp/vfs_mount
./build.sh fuse:stop    # unmount FUSE FS
./build.sh fuse:status  # show mount status
./build.sh fuse:shell   # open a subshell at /tmp/vfs_mount
./build.sh demo         # build → mount → list → unmount
```

If you see `'/usr/bin/env: bash\r: No such file or directory'`, convert Windows line endings to Unix:
```bash
dos2unix build.sh
```

## Building and Testing
```powershell
# Build everything
wsl bash -c "./build.sh build"

# Run tests
wsl bash -c "./build.sh test"

# Memory leak detection (valgrind)
wsl bash -c "./build.sh valgrind"
```

FUSE test scripts (optional, already verified):
```powershell
wsl bash tests/test_fuse_listing.sh
wsl bash tests/test_fuse_nested.sh
wsl bash tests/test_fuse_stability.sh
wsl bash tests/test_fuse_complete.sh
```

## Architecture Overview
- **VFS Core (`src/core/`)**: Implements core filesystem abstractions, path resolution, readdir, stat, and lifecycle management with strict reference counting (inodes/dentries).
- **Backends (`src/backends/`)**: The POSIX backend performs real file I/O against a directory tree, mounted via `vfs_mount_backend`.
- **FUSE Layer (`src/fuse/`)**: Adapts VFS APIs to FUSE3 callbacks. Notably, `readdir` uses the FUSE3 5-parameter filler signature for compatibility.
- **Tools (`src/tools/`)**: CLI helpers and small utilities.

## Quality and Validation
- Unit, integration, and stress tests all passing
- Valgrind reports zero leaks across all suites
- FUSE directory listing and stat operations verified under WSL
- Thread safety validated with 1000 concurrent operations

## Troubleshooting
- **Unmount fails (busy):** Ensure you `exit` any shell currently inside `/tmp/vfs_mount` before running `./build.sh fuse:stop`.
- **FUSE not mounting:** Verify prerequisites and that WSL supports FUSE. Check logs: `tail -100 /tmp/fuse_run.log`.
- **Windows line endings in scripts:** Run `dos2unix build.sh tests/*.sh` inside WSL.
- **Permissions:** If operations fail, confirm your WSL user has rights to `/tmp/*` and the backend directories.

## License
This repository is for educational use. If you plan to redistribute or publish, please add an appropriate license file to the root.

## Acknowledgements
Built as a structured exploration of VFS design, FUSE integration, and systems testing under WSL.

## Future Goals
Implements Hybrid and dynamic algorithms to manage cache and memory frames to extend degree of multi programing of cpu
