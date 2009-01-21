#include <stdio.h>
#include <assert.h>
#include <stdint.h>
#include <unistd.h>
#include <windows.h>

int64_t
pread(int fildes, void *buf, size_t nbyte, int64_t offset) {
    HANDLE     filehandle;
    OVERLAPPED win_offset = {0}; 
    filehandle = (HANDLE)_get_osfhandle(fildes);
    int64_t r;
    if (filehandle==INVALID_HANDLE_VALUE) {
        r = errno; assert(r!=0);
        goto cleanup;
    }
    win_offset.Offset     = offset % (1LL<<32LL);
    win_offset.OffsetHigh = offset / (1LL<<32LL);

    DWORD bytes_read;
    r = ReadFile(filehandle, buf, nbyte, &bytes_read, &win_offset);
    if (!r) {
        r = GetLastError();
        if (r==ERROR_HANDLE_EOF) r = bytes_read;
        else {
            errno = r;
            r = -1;
        }
    }
    else    r = bytes_read;

    // printf("%s: %d %p %u %I64d %I64d\n", __FUNCTION__, fildes, buf, nbyte, offset, r); fflush(stdout);
cleanup:
    return r;
}

int64_t
pwrite(int fildes, const void *buf, size_t nbyte, int64_t offset) {
    HANDLE     filehandle;
    OVERLAPPED win_offset = {0}; 
    filehandle = (HANDLE)_get_osfhandle(fildes);
    int64_t r;
    if (filehandle==INVALID_HANDLE_VALUE) {
        r = errno; assert(r!=0);
        goto cleanup;
    }
    win_offset.Offset     = offset % (1LL<<32LL);
    win_offset.OffsetHigh = offset / (1LL<<32LL);

    DWORD bytes_written;
    r = WriteFile(filehandle, buf, nbyte, &bytes_written, &win_offset);
    if (!r) {
        errno = GetLastError();
        r = -1;
    }
    else    r = bytes_written;

    // printf("%s: %d %p %u %I64d %I64d\n", __FUNCTION__, fildes, buf, nbyte, offset, r); fflush(stdout);
cleanup:
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

static ssize_t (*t_pwrite)(int, const void *, size_t, off_t) = 0;
static ssize_t (*t_write)(int, const void *, size_t) = 0;

int toku_set_func_pwrite (ssize_t (*pwrite_fun)(int, const void *, size_t, off_t)) {
    t_pwrite = pwrite_fun;
    return 0;
}

int toku_set_func_write (ssize_t (*write_fun)(int, const void *, size_t)) {
    t_write = write_fun;
    return 0;
}


ssize_t
toku_os_pwrite (int fd, const void *buf, size_t len, off_t off)
{
    if (t_pwrite) {
	return t_pwrite(fd, buf, len, off);
    } else {
	return pwrite(fd, buf, len, off);
    }
}

