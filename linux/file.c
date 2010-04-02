/* -*- mode: C; c-basic-offset: 4 -*- */
#include <toku_portability.h>
#include <toku_atomic.h>
#include <unistd.h>
#include <errno.h>
#include <toku_assert.h>
#include <stdio.h>
#include <string.h>
#include <time.h>
#include <dirent.h>

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

                const int MY_MAX_PATH = 256;
                char fname[MY_MAX_PATH], symname[MY_MAX_PATH];
                sprintf(fname, "/proc/%d/fd/%d", getpid(), fd);
                ssize_t n = readlink(fname, symname, MY_MAX_PATH);

                if ((int)n == -1)
                    fprintf(stderr, "%.24s Tokudb No space when writing %"PRIu64" bytes to fd=%d ", tstr, (uint64_t) len, fd);
                else
                    fprintf(stderr, "%.24s Tokudb No space when writing %"PRIu64" bytes to %*s ", tstr, (uint64_t) len, (int) n, symname); 
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

static ssize_t (*t_pwrite)(int, const void *, size_t, off_t) = 0;

int 
toku_set_func_pwrite (ssize_t (*pwrite_fun)(int, const void *, size_t, off_t)) {
    t_pwrite = pwrite_fun;
    return 0;
}

void
toku_os_full_pwrite (int fd, const void *buf, size_t len, off_t off) {
    const char *bp = (const char *) buf;
    while (len > 0) {
        ssize_t r;
        if (t_pwrite) {
            r = t_pwrite(fd, bp, len, off);
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

static ssize_t (*t_write)(int, const void *, size_t) = 0;

int 
toku_set_func_write (ssize_t (*write_fun)(int, const void *, size_t)) {
    t_write = write_fun;
    return 0;
}

void
toku_os_full_write (int fd, const void *buf, size_t len) {
    const char *bp = (const char *) buf;
    while (len > 0) {
        ssize_t r;
        if (t_write) {
            r = t_write(fd, bp, len);
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
    const char *bp = (const char *) buf;
    while (len > 0) {
        ssize_t r;
        if (t_write) {
            r = t_write(fd, bp, len);
        } else {
            r = write(fd, bp, len);
        }
        if (r < 0)
            return errno;
        len           -= r;
        bp            += r;
    }
    return 0;
}

/////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////////
// fsync logic:

// t_fsync exists for testing purposes only
static int (*t_fsync)(int) = 0;
static uint64_t toku_fsync_count;
static uint64_t toku_fsync_time;

static uint64_t sched_fsync_count;
static uint64_t sched_fsync_time;

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
file_fsync_internal (int fd, uint64_t *duration_p) {
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
    toku_sync_fetch_and_increment_uint64(&toku_fsync_count);
    uint64_t duration;
    duration = get_tnow() - tstart;
    toku_sync_fetch_and_add_uint64(&toku_fsync_time, duration);
    if (duration_p) *duration_p = duration;
    return r;
}

int
toku_file_fsync_without_accounting (int fd) {
    int r = file_fsync_internal (fd, NULL);
    return r;
}


int
toku_fsync_dirfd_without_accounting(DIR *dirp) {
    int r;
    int fd = dirfd(dirp);
    if (fd < 0) {
        r = -1;
    } else {
        r = toku_file_fsync_without_accounting(fd);
    }
    return r;
}

// include fsync in scheduling accounting
int
toku_file_fsync(int fd) {
    uint64_t duration;
    int r = file_fsync_internal (fd, &duration);
    toku_sync_fetch_and_increment_uint64(&sched_fsync_count);
    toku_sync_fetch_and_add_uint64(&sched_fsync_time, duration);
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
