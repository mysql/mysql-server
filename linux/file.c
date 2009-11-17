/* -*- mode: C; c-basic-offset: 4 -*- */
#include <toku_portability.h>
#include <toku_atomic.h>
#include <unistd.h>
#include <errno.h>
#include <assert.h>
#include <stdio.h>

//Print any necessary errors
//Return whether we should try the write again.
static int
try_again_after_handling_write_error(int fd, size_t len, ssize_t r_write) {
    int try_again = 0;

    if (r_write==-1) {
        int errno_write = errno;
        assert(errno_write != 0);
        switch (errno_write) {
            case EINTR: { //The call was interrupted by a signal before any data was written; see signal(7).
                char err_msg[sizeof("Write of [] bytes to fd=[] interrupted.  Retrying.") + 20+10]; //64 bit is 20 chars, 32 bit is 10 chars
                snprintf(err_msg, sizeof(err_msg), "Write of [%"PRIu64"] bytes to fd=[%d] interrupted.  Retrying.", (uint64_t)len, fd);
                perror(err_msg);
                fflush(stdout);
                try_again = 1;
                break;
            }
            case ENOSPC: {
                char err_msg[sizeof("Failed write of [] bytes to fd=[].") + 20+10]; //64 bit is 20 chars, 32 bit is 10 chars
                snprintf(err_msg, sizeof(err_msg), "Failed write of [%"PRIu64"] bytes to fd=[%d].", (uint64_t)len, fd);
                perror(err_msg);
                fflush(stdout);
                int out_of_disk_space = 1;
                assert(!out_of_disk_space); //Give an error message that might be useful if this is the only one that survives.
            }
            default:
                break;
        }
        errno = errno_write;
    }
    return try_again;
}

static ssize_t (*t_pwrite)(int, const void *, size_t, off_t) = 0;

int 
toku_set_func_pwrite (ssize_t (*pwrite_fun)(int, const void *, size_t, off_t)) {
    t_pwrite = pwrite_fun;
    return 0;
}

ssize_t
toku_os_pwrite (int fd, const void *buf, size_t len, off_t off) {
    ssize_t r;
again:
    if (t_pwrite) {
	r = t_pwrite(fd, buf, len, off);
    } else {
	r = pwrite(fd, buf, len, off);
    }
    if (try_again_after_handling_write_error(fd, len, r))
        goto again;
    return r;
}

static ssize_t (*t_write)(int, const void *, size_t) = 0;

int 
toku_set_func_write (ssize_t (*write_fun)(int, const void *, size_t)) {
    t_write = write_fun;
    return 0;
}

ssize_t
toku_os_write (int fd, const void *buf, size_t len) {
    ssize_t r;
again:
    if (t_pwrite) {
	r = t_write(fd, buf, len);
    } else {
	r = write(fd, buf, len);
    }
    if (try_again_after_handling_write_error(fd, len, r))
        goto again;
    return r;
}

static uint64_t get_tnow(void) {
    struct timeval tv;
    int r = gettimeofday(&tv, NULL); assert(r == 0);
    return tv.tv_sec * 1000000ULL + tv.tv_usec;
}

// t_fsync exists for testing purposes only
static int (*t_fsync)(int) = 0;
static uint64_t toku_fsync_count;
static uint64_t toku_fsync_time;

int
toku_set_func_fsync(int (*fsync_function)(int)) {
    t_fsync = fsync_function;
    return 0;
}

// keep trying if fsync fails because of EINTR
int
toku_file_fsync(int fd) {
    int r = -1;
    uint64_t tstart = get_tnow();
    while (r != 0) {
	if (t_fsync)
	    r = t_fsync(fd);
	else 
	    r = fsync(fd);
	if (r) 
	    assert(errno==EINTR);
    }
    toku_sync_fetch_and_add_uint64(&toku_fsync_count, 1);
    toku_sync_fetch_and_add_uint64(&toku_fsync_time, get_tnow() - tstart);
    return r;
}

void
toku_get_fsync_times(uint64_t *fsync_count, uint64_t *fsync_time) {
    *fsync_count = toku_fsync_count;
    *fsync_time = toku_fsync_time;
}

