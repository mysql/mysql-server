#include <toku_portability.h>
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
        if (errno == ERROR_HANDLE_DISK_FULL ||
            errno == ERROR_DISK_FULL) {
            char err_msg[sizeof("Failed write of [] bytes to fd=[].") + 20+10]; //64 bit is 20 chars, 32 bit is 10 chars
            snprintf(err_msg, sizeof(err_msg), "Failed write of [%"PRIu64"] bytes to fd=[%d].", nbyte, fildes);
            perror(err_msg);
            fflush(stdout);
            int out_of_disk_space = 1;
            assert(!out_of_disk_space); //Give an error message that might be useful if this is the only one that survives.
        }

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

static ssize_t (*t_pwrite)(int, const void *, size_t, toku_off_t) = 0;
static ssize_t (*t_write)(int, const void *, size_t) = 0;

int toku_set_func_pwrite (ssize_t (*pwrite_fun)(int, const void *, size_t, toku_off_t)) {
    t_pwrite = pwrite_fun;
    return 0;
}

int toku_set_func_write (ssize_t (*write_fun)(int, const void *, size_t)) {
    t_write = write_fun;
    t_write = t_write; //So far unused
    return 0;
}


void
toku_os_full_pwrite (int fd, const void *buf, size_t len, toku_off_t off)
{
    ssize_t r;
    if (t_pwrite) {
	r = t_pwrite(fd, buf, len, off);
    } else {
	r = pwrite(fd, buf, len, off);
    }
    assert(r==len);
}

static int (*t_fsync)(int) = 0;
static uint64_t toku_fsync_count;
static uint64_t toku_fsync_time;

int
toku_set_func_fsync(int (*fsync_function)(int)) {
    t_fsync = fsync_function;
    return 0;
}

int
toku_file_fsync(int fd) {
    int r;
    if (t_fsync)
        r = t_fsync(fd);
    else
        r = fsync(fd);
    return r;
}

void
toku_get_fsync_times(uint64_t *fsync_count, uint64_t *fsync_time) {
    *fsync_count = toku_fsync_count;
    *fsync_time = toku_fsync_time;
}

