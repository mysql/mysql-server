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


static ssize_t (*t_write)(int, const void *, size_t) = NULL;
static ssize_t (*t_full_write)(int, const void *, size_t) = NULL;
static ssize_t (*t_pwrite)(int, const void *, size_t, toku_off_t) = NULL;
static ssize_t (*t_full_pwrite)(int, const void *, size_t, toku_off_t) = NULL;
static FILE *  (*t_fdopen)(int, const char *) = NULL;
static FILE *  (*t_fopen)(const char *, const char *) = NULL;
static int     (*t_open)(const char *, int, int) = NULL;  // no implementation of variadic form until needed
static int     (*t_fclose)(FILE *) = NULL;

int 
toku_set_func_write (ssize_t (*write_fun)(int, const void *, size_t)) {
    t_write = write_fun;
    return 0;
}

int 
toku_set_func_full_write (ssize_t (*write_fun)(int, const void *, size_t)) {
    t_full_write = write_fun;
    return 0;
}

int 
toku_set_func_pwrite (ssize_t (*pwrite_fun)(int, const void *, size_t, toku_off_t)) {
    t_pwrite = pwrite_fun;
    return 0;
}

int 
toku_set_func_full_pwrite (ssize_t (*pwrite_fun)(int, const void *, size_t, toku_off_t)) {
    t_full_pwrite = pwrite_fun;
    return 0;
}

int 
toku_set_func_fdopen(FILE * (*fdopen_fun)(int, const char *)) {
    t_fdopen = fdopen_fun;
    return 0;
}


int 
toku_set_func_fopen(FILE * (*fopen_fun)(const char *, const char *)) {
    t_fopen = fopen_fun;
    return 0;
}


int 
toku_set_func_open(int (*open_fun)(const char *, int, int)) {
    t_open = open_fun;
    return 0;
}

int 
toku_set_func_fclose(int (*fclose_fun)(FILE*)) {
    t_fclose = fclose_fun;
    return 0;
}





static int toku_assert_on_write_enospc = 0;
static const int toku_write_enospc_sleep = 1;
static uint64_t toku_write_enospc_last_report;      // timestamp of most recent report to error log
static time_t   toku_write_enospc_last_time;        // timestamp of most recent ENOSPC
static uint32_t toku_write_enospc_current;          // number of threads currently blocked on ENOSPC
static uint64_t toku_write_enospc_total;            // total number of times ENOSPC was returned from an attempt to write

void toku_set_assert_on_write_enospc(int do_assert) {
    toku_assert_on_write_enospc = do_assert;
}

void
toku_fs_get_write_info(time_t *enospc_last_time, uint64_t *enospc_current, uint64_t *enospc_total) {
    *enospc_last_time = toku_write_enospc_last_time;
    *enospc_current = toku_write_enospc_current;
    *enospc_total = toku_write_enospc_total;
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
        if (toku_assert_on_write_enospc) {
            char err_msg[sizeof("Failed write of [] bytes to fd=[].") + 20+10]; //64 bit is 20 chars, 32 bit is 10 chars
            snprintf(err_msg, sizeof(err_msg), "Failed write of [%"PRIu64"] bytes to fd=[%d].", (uint64_t)len, fd);
            perror(err_msg);
            fflush(stderr);
            int out_of_disk_space = 1;
            assert(!out_of_disk_space); //Give an error message that might be useful if this is the only one that survives.
        } else {
            toku_sync_fetch_and_increment_uint64(&toku_write_enospc_total);
            toku_sync_fetch_and_increment_uint32(&toku_write_enospc_current);

            time_t tnow = time(0);
            toku_write_enospc_last_time = tnow;
            if (toku_write_enospc_last_report == 0 || tnow - toku_write_enospc_last_report >= 60) {
                toku_write_enospc_last_report = tnow;

                const int tstr_length = 26;
                char tstr[tstr_length];
                time_t t = time(0);
                ctime_r(&t, tstr);

#if 0
                //TODO: Find out how to get name from fd or handle
                //In vista/server08 and later there exists GetFinalPathNameByHandle()
                //In XP there exists:
                //  Example code that requires on kernel-level functions that
                //  are not guaranteed to stay around.
                //    http://rubyforge.org/pipermail/win32utils-devel/2008-May/001091.html
                //  Example code that works for files of length > 0 (according
                //  to author).  This one appears to not require unsafe apis
                //  (not guaranteed to stick around)
                //    http://msdn.microsoft.com/en-us/library/aa366789%28VS.85%29.aspx
                //  Can we use runtime-checks to determine what version of OS we
                //  are using so we can choose which function to use?
                //  We CAN do compile-time checks, but then we would need to
                //  release multiple versions of binary.
                //  Is this important?
                //
                const int MY_MAX_PATH = 256;
                char fname[MY_MAX_PATH], symname[MY_MAX_PATH];
                sprintf(fname, "/proc/%d/fd/%d", getpid(), fd);
                ssize_t n = readlink(fname, symname, MY_MAX_PATH);
                if ((int)n == -1)
                    fprintf(stderr, "%.24s Tokudb No space when writing %"PRIu64" bytes to fd=%d ", tstr, (uint64_t) len, fd);
                else
                    fprintf(stderr, "%.24s Tokudb No space when writing %"PRIu64" bytes to %*s ", tstr, (uint64_t) len, (int) n, symname); 
#else
                fprintf(stderr, "%.24s Tokudb No space when writing %"PRIu64" bytes to fd=%d ", tstr, (uint64_t) len, fd);
#endif

                fprintf(stderr, "retry in %d second%s\n", toku_write_enospc_sleep, toku_write_enospc_sleep > 1 ? "s" : "");
                fflush(stderr);
            }
            sleep(toku_write_enospc_sleep);
            try_again = 1;
            toku_sync_fetch_and_decrement_uint32(&toku_write_enospc_current);
            break;
        }
    }
    default:
	break;
    }
    assert(try_again);
    errno = errno_write;
}


void
toku_os_full_write (int fd, const void *buf, size_t len) {
    const uint8_t *bp = (const uint8_t *) buf;
    while (len > 0) {
        ssize_t r;
        if (t_full_write) {
            r = t_full_write(fd, bp, len);
        } else {
            r = write(fd, bp, len);
        }
        if (r > 0) {
            len           -= r;
            bp            += r;
        }
        else {
            try_again_after_handling_write_error(fd, len, r);
        }
    }
    assert(len == 0);
}

int
toku_os_write (int fd, const void *buf, size_t len) {
    const uint8_t *bp = (const uint8_t *) buf;
    int result = 0;
    while (len > 0) {
        ssize_t r;
        if (t_write) {
            r = t_write(fd, bp, len);
        } else {
            r = write(fd, bp, len);
        }
        if (r < 0) {
            result = errno;
            break;
        }
        len           -= r;
        bp            += r;
    }
    return result;
}

void
toku_os_full_pwrite (int fd, const void *buf, size_t len, toku_off_t off) {
    const uint8_t *bp = (const uint8_t *) buf;
    while (len > 0) {
        ssize_t r;
        if (t_full_pwrite) {
            r = t_full_pwrite(fd, bp, len, off);
        } else {
            r = pwrite(fd, bp, len, off);
        }
        if (r > 0) {
            len           -= r;
            bp            += r;
            off           += r;
        }
        else {
            try_again_after_handling_write_error(fd, len, r);
        }
    }
    assert(len == 0);
}

ssize_t
toku_os_pwrite (int fd, const void *buf, size_t len, toku_off_t off) {
    const uint8_t *bp = (const uint8_t *) buf;
    ssize_t result = 0;
    while (len > 0) {
        ssize_t r;
        if (t_pwrite) {
            r = t_pwrite(fd, bp, len, off);
        } else {
            r = pwrite(fd, bp, len, off);
        }
        if (r < 0) {
            result = errno;
            break;
        }
        len           -= r;
        bp            += r;
        off           += r;
    }
    return result;
}

FILE * 
toku_os_fdopen(int fildes, const char *mode) {
    FILE * rval;
    if (t_fdopen)
	rval = t_fdopen(fildes, mode);
    else 
	rval = fdopen(fildes, mode);
    return rval;
}
    

FILE *
toku_os_fopen(const char *filename, const char *mode){
    FILE * rval;
    if (t_fopen)
	rval = t_fopen(filename, mode);
    else
	rval = fopen(filename, mode);
    return rval;
}

int 
toku_os_open(const char *path, int oflag, int mode) {
    int rval;
    if (t_open)
	rval = t_open(path, oflag, mode);
    else
	rval = open(path, oflag, mode);
    return rval;
}

int
toku_os_fclose(FILE * stream) {  
    int rval = -1;
    if (t_fclose)
	rval = t_fclose(stream);
    else {                      // if EINTR, retry until success
	while (rval != 0) {
	    rval = fclose(stream);
	    if (rval && (errno != EINTR))
		break;
	}
    }
    return rval;
}

int 
toku_os_close (int fd) {  // if EINTR, retry until success
    int r = -1;
    while (r != 0) {
	r = close(fd);
	if (r) {
	    int rr = errno;
	    if (rr!=EINTR) printf("rr=%d (%s)\n", rr, strerror(rr));
	    assert(rr==EINTR);
	}
    }
    return r;
}




/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// fsync logic:


// t_fsync exists for testing purposes only
static int (*t_fsync)(int) = 0;
static uint64_t toku_fsync_count;
static uint64_t toku_fsync_time;

static uint64_t sched_fsync_count;
static uint64_t sched_fsync_time;

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
static int 
toku_file_fsync_internal (int fd, uint64_t *duration_p) {
    uint64_t tstart = get_tnow();
    int r = -1;
    while (r != 0) {
	if (t_fsync)
	    r = t_fsync(fd);
	else 
	    r = fsync(fd);
	if (r) {
	    int rr = errno;
	    if (rr!=EINTR) printf("rr=%d (%s)\n", rr, strerror(rr));
	    assert(rr==EINTR);
	}
    }
    uint64_t duration;
    duration = get_tnow() - tstart;
#if TOKU_WINDOWS_HAS_ATOMIC_64 
    toku_sync_fetch_and_increment_uint64(&toku_fsync_count);
    toku_sync_fetch_and_add_uint64(&toku_fsync_time, duration);
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
    toku_fsync_time += duration;
    r_mutex = toku_pthread_mutex_unlock(&fsync_lock); assert(r_mutex == 0);
#endif
    if (duration_p) *duration_p = duration;
    return r;
}

// keep trying if fsync fails because of EINTR
int
toku_file_fsync_without_accounting (int fd) {
    int r = toku_file_fsync_internal(fd, NULL);
    return r;
}

int
toku_file_fsync(int fd) {
    uint64_t duration;
    int r = toku_file_fsync_internal(fd, &duration);
#if TOKU_WINDOWS_HAS_ATOMIC_64 
    toku_sync_fetch_and_increment_uint64(&sched_fsync_count);
    toku_sync_fetch_and_add_uint64(&sched_fsync_time, duration);
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
    sched_fsync_count++;
    sched_fsync_time += duration;
    r_mutex = toku_pthread_mutex_unlock(&fsync_lock); assert(r_mutex == 0);
#endif
    return r;
}

// for real accounting
void
toku_get_fsync_times(uint64_t *fsync_count, uint64_t *fsync_time) {
    *fsync_count = toku_fsync_count;
    *fsync_time = toku_fsync_time;
}

// for scheduling algorithm only
void
toku_get_fsync_sched(uint64_t *fsync_count, uint64_t *fsync_time) {
    *fsync_count = sched_fsync_count;
    *fsync_time  = sched_fsync_time;
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

ssize_t 
toku_os_read(int fd, void *buf, size_t count) {
    return read(fd, buf, count);
}




