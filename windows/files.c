#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <unistd.h>
#include <windows.h>

int64_t
pread(int fildes, void *buf, size_t nbyte, int64_t offset) {
    HANDLE     handle;
    OVERLAPPED offset = {0}; 
    handle =  _get_osfhandle(fildes);
    offset.Offset     = offset % (1LL<<32LL);
    offset.OffsetHigh = offset / (1LL<<32LL);

    size_t bytes_read;
    int64_t r = ReadFile(handle, buf, nbyte, &bytes_read, &offset);
    if (!r) r = GetLastError();
    else    r = bytes_read;

    // printf("%s: %d %p %u %I64d %I64d\n", __FUNCTION__, fildes, buf, nbyte, offset, r); fflush(stdout);
    return r;
}

int64_t
pwrite(int fildes, const void *buf, size_t nbyte, int64_t offset) {
    HANDLE     handle;
    OVERLAPPED offset = {0}; 
    handle =  _get_osfhandle(fildes);
    offset.Offset     = offset % (1LL<<32LL);
    offset.OffsetHigh = offset / (1LL<<32LL);

    size_t bytes_written;
    int64_t r = ReadFile(handle, buf, nbyte, &bytes_written, &offset);
    if (!r) r = GetLastError();
    else    r = bytes_written;

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

