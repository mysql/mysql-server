#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <unistd.h>
#include <windows.h>

// Note: pread and pwrite are not thread safe on the same fildes as they
// rely on the file offset

int64_t
pread(int fildes, void *buf, size_t nbyte, int64_t offset) {
    int64_t r = _lseeki64(fildes, offset, SEEK_SET);
    if (r>=0) {
        assert(r==offset);
        r = read(fildes, buf, nbyte);
    }
    // printf("%s: %d %p %u %I64d %I64d\n", __FUNCTION__, fildes, buf, nbyte, offset, r); fflush(stdout);
    return r;
}

int64_t
pwrite(int fildes, const void *buf, size_t nbyte, int64_t offset) {
    int64_t r = _lseeki64(fildes, offset, SEEK_SET);
    if (r>=0) {
        assert(r==offset);
        r = write(fildes, buf, nbyte);
    }
    // printf("%s: %d %p %u %I64d %I64d\n", __FUNCTION__, fildes, buf, nbyte, offset, r); fflush(stdout);
    return r;
}

int
fsync(int fd) {
    int r = _commit(fd);
    return r;
}

int 
ftruncate(int fd, int64_t offset) {
    int r = _chsize_s(fd, offset);
    return r;
}

