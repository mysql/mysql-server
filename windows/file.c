#include <toku_portability.h>
#include <stdio.h>
#include <toku_assert.h>
#include <stdint.h>
#include <unistd.h>
#include <windows.h>
#include <toku_atomic.h>
#include <toku_time.h>
#include <fcntl.h>

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
        r = -1;
        assert(errno!=0);
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
            errno = ENOSPC;
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
ftruncate(int fd, toku_off_t offset) {
    int r = _chsize_s(fd, offset);
    if (r!=0) {
        r = -1;
        assert(errno!=0);
    }
    return r;
}

int
truncate(const char *path, toku_off_t length) {
    int r;
    int saved_errno;
    int fd = open(path, _O_BINARY|_O_RDWR, _S_IREAD|_S_IWRITE);
    if (fd<0) {
        r = -1;
        goto done;
    }
    r = ftruncate(fd, length);
    saved_errno = errno;
    if (r!=0) {
        r = -1;
        assert(errno!=0);
    }
    int r2 = close(fd);
    if (r==0) {
        r = r2;
    }
    else {
        errno = saved_errno;
    }
done:
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

//Print any necessary errors
//Return whether we should try the write again.
static void
try_again_after_handling_write_error(int fd, size_t len, ssize_t r_write) {
    int try_again = 0;

    assert(r_write < 0);
    int errno_write = errno;
    assert(errno_write != 0);
    switch (errno_write) {
        case EINTR: { //The call was interrupted by a signal before any data was written; see signal(7).
            char err_msg[sizeof("Write of [] bytes to fd=[] interrupted.  Retrying.") + 20+10]; //64 bit is 20 chars, 32 bit is 10 chars
            snprintf(err_msg, sizeof(err_msg), "Write of [%"PRIu64"] bytes to fd=[%d] interrupted.  Retrying.", (uint64_t)len, fd);
            perror(err_msg);
            fflush(stderr);
            try_again = 1;
            break;
        }
        case ENOSPC: {
            char err_msg[sizeof("Failed write of [] bytes to fd=[].") + 20+10]; //64 bit is 20 chars, 32 bit is 10 chars
            snprintf(err_msg, sizeof(err_msg), "Failed write of [%"PRIu64"] bytes to fd=[%d].", (uint64_t)len, fd);
            perror(err_msg);
            fflush(stderr);
            int out_of_disk_space = 1;
            assert(!out_of_disk_space); //Give an error message that might be useful if this is the only one that survives.
        }
        default:
            break;
    }
    assert(try_again);
    errno = errno_write;
}

void
toku_os_full_pwrite (int fd, const void *org_buf, size_t len, toku_off_t off)
{
    const uint8_t *buf = org_buf;
    while (len > 0) {
        ssize_t r;
        if (t_pwrite) {
            r = t_pwrite(fd, buf, len, off);
        } else {
            r = pwrite(fd, buf, len, off);
        }
        if (r > 0) {
            len           -= r;
            buf           += r;
            off           += r;
        }
        else {
            try_again_after_handling_write_error(fd, len, r);
        }
    }
    assert(len == 0);
}
/*
{
    ssize_t r;
    if (t_pwrite) {
	r = t_pwrite(fd, buf, len, off);
    } else {
	r = pwrite(fd, buf, len, off);
    }
    if (r==-1 && errno==ENOSPC) {
        char err_msg[sizeof("Failed write of [] bytes to fd=[].") + 20+10]; //64 bit is 20 chars, 32 bit is 10 chars
        snprintf(err_msg, sizeof(err_msg), "Failed write of [%"PRIu64"] bytes to fd=[%d].", len, fd);
        perror(err_msg);
        fflush(stderr);
        int out_of_disk_space = 1;
        assert(!out_of_disk_space); //Give an error message that might be useful if this is the only one that survives.
    }
    assert(r==len);
}
*/
void
toku_os_full_write (int fd, const void *org_buf, size_t len) {
    const uint8_t *buf = org_buf;
    while (len > 0) {
        ssize_t r;
        if (t_write) {
            r = t_write(fd, buf, len);
        } else {
            r = write(fd, buf, len);
        }
        if (r > 0) {
            len           -= r;
            buf           += r;
        }
        else {
            try_again_after_handling_write_error(fd, len, r);
        }
    }
    assert(len == 0);
}

int
toku_os_write (int fd, const void *org_buf, size_t len) {
    const uint8_t *buf = org_buf;
    while (len > 0) {
        ssize_t r;
        if (t_write) {
            r = t_write(fd, buf, len);
        } else {
            r = write(fd, buf, len);
        }
        if (r < 0)
            return errno;
        len           -= r;
        buf           += r;
    }
    return 0;
}


// t_fsync exists for testing purposes only
static int (*t_fsync)(int) = 0;
static uint64_t toku_fsync_count;
static uint64_t toku_fsync_time;

#if !TOKU_WINDOWS_HAS_ATOMIC_64 
static toku_pthread_mutex_t fsync_lock;
#endif

int
toku_fsync_init(void) {
    int r = 0;
#if !TOKU_WINDOWS_HAS_ATOMIC_64 
    r = toku_pthread_mutex_init(&fsync_lock, NULL); assert(r == 0);
#endif
    return r;
}

int
toku_fsync_destroy(void) {
    int r = 0;
#if !TOKU_WINDOWS_HAS_ATOMIC_64 
    r = toku_pthread_mutex_destroy(&fsync_lock); assert(r == 0);
#endif
    return r;
}

int
toku_set_func_fsync(int (*fsync_function)(int)) {
    t_fsync = fsync_function;
    return 0;
}
static uint64_t get_tnow(void) {
    struct timeval tv;
    int r = gettimeofday(&tv, NULL); assert(r == 0);
    return tv.tv_sec * 1000000ULL + tv.tv_usec;
}

// keep trying if fsync fails because of EINTR
int
toku_file_fsync_without_accounting (int fd) {
    int r = -1;
    while (r != 0) {
	if (t_fsync)
	    r = t_fsync(fd);
	else 
	    r = fsync(fd);
	if (r) 
	    assert(errno==EINTR);
    }
    return r;
}

int
toku_file_fsync(int fd) {
    uint64_t tstart = get_tnow();
    int r = toku_file_fsync_without_accounting(fd);
#if TOKU_WINDOWS_HAS_ATOMIC_64 
    toku_sync_fetch_and_increment_uint64(&toku_fsync_count);
    toku_sync_fetch_and_add_uint64(&toku_fsync_time, get_tnow() - tstart);
#else
    //These two need to be fully 64 bit and atomic.
    //The windows atomic add 64 bit is not available.
    //toku_sync_fetch_and_add_uint64 (and increment) treat it as 32 bit, and
    //would overflow.
    //Even on 32 bit machines, aligned 64 bit writes/writes are atomic, so we just
    //need to make sure there's only one writer for these two variables.
    //Protect with a mutex. Fsync is rare/slow enough that this should be ok.
    int r_mutex;
    r_mutex = toku_pthread_mutex_lock(&fsync_lock);   assert(r_mutex == 0);
    toku_fsync_count++;
    toku_fsync_time += get_tnow() - tstart;
    r_mutex = toku_pthread_mutex_unlock(&fsync_lock); assert(r_mutex == 0);
#endif
    return r;
}

void
toku_get_fsync_times(uint64_t *fsync_count, uint64_t *fsync_time) {
    *fsync_count = toku_fsync_count;
    *fsync_time = toku_fsync_time;
}

static toku_pthread_mutex_t mkstemp_lock;

int
toku_mkstemp_init(void) {
    int r = 0;
    r = toku_pthread_mutex_init(&mkstemp_lock, NULL); assert(r == 0);
    return r;
}

int
toku_mkstemp_destroy(void) {
    int r = 0;
    r = toku_pthread_mutex_destroy(&mkstemp_lock); assert(r == 0);
    return r;
}

int mkstemp (char * template) {
    int fd;
    int r_mutex;
    r_mutex = toku_pthread_mutex_lock(&mkstemp_lock);
    assert(r_mutex == 0);
    errno_t err = _mktemp_s(template, strlen(template)+1);
    if (err!=0) {
        fd = -1;
        errno = err;
        goto cleanup;
    }
    assert(err==0);
    fd = open(template, _O_BINARY|_O_CREAT|_O_SHORT_LIVED|_O_EXCL|_O_RDWR, _S_IREAD|_S_IWRITE);
cleanup:
    r_mutex = toku_pthread_mutex_unlock(&mkstemp_lock);
    assert(r_mutex == 0);
    return fd;
}

toku_off_t
ftello(FILE *stream) {
    toku_off_t offset = _ftelli64(stream);
    return offset;
}

