/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."

#include <toku_portability.h>
#include <unistd.h>
#include <errno.h>
#include <toku_assert.h>
#include <stdio.h>
#include <string.h>
#include <dirent.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "memory.h"
#include "toku_time.h"

static int toku_assert_on_write_enospc = 0;
static const int toku_write_enospc_sleep = 1;
static uint64_t toku_write_enospc_last_report;      // timestamp of most recent report to error log
static time_t   toku_write_enospc_last_time;        // timestamp of most recent ENOSPC
static uint32_t toku_write_enospc_current;          // number of threads currently blocked on ENOSPC
static uint64_t toku_write_enospc_total;            // total number of times ENOSPC was returned from an attempt to write

void toku_set_assert_on_write_enospc(int do_assert) {
    toku_assert_on_write_enospc = do_assert;
}

void toku_fs_get_write_info(time_t *enospc_last_time, uint64_t *enospc_current, uint64_t *enospc_total) {
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
    int errno_write = get_error_errno();
    switch (errno_write) {
    case EINTR: { //The call was interrupted by a signal before any data was written; see signal(7).
	char err_msg[sizeof("Write of [] bytes to fd=[] interrupted.  Retrying.") + 20+10]; //64 bit is 20 chars, 32 bit is 10 chars
	snprintf(err_msg, sizeof(err_msg), "Write of [%" PRIu64 "] bytes to fd=[%d] interrupted.  Retrying.", (uint64_t)len, fd);
	perror(err_msg);
	fflush(stderr);
	try_again = 1;
	break;
    }
    case ENOSPC: {
        if (toku_assert_on_write_enospc) {
            char err_msg[sizeof("Failed write of [] bytes to fd=[].") + 20+10]; //64 bit is 20 chars, 32 bit is 10 chars
            snprintf(err_msg, sizeof(err_msg), "Failed write of [%" PRIu64 "] bytes to fd=[%d].", (uint64_t)len, fd);
            perror(err_msg);
            fflush(stderr);
            int out_of_disk_space = 1;
            assert(!out_of_disk_space); //Give an error message that might be useful if this is the only one that survives.
        } else {
            __sync_fetch_and_add(&toku_write_enospc_total, 1);
            __sync_fetch_and_add(&toku_write_enospc_current, 1);

            time_t tnow = time(0);
            toku_write_enospc_last_time = tnow;
            if (toku_write_enospc_last_report == 0 || tnow - toku_write_enospc_last_report >= 60) {
                toku_write_enospc_last_report = tnow;

                const int tstr_length = 26;
                char tstr[tstr_length];
                time_t t = time(0);
                ctime_r(&t, tstr);

                const int MY_MAX_PATH = 256;
                char fname[MY_MAX_PATH], symname[MY_MAX_PATH+1];
                sprintf(fname, "/proc/%d/fd/%d", getpid(), fd);
                ssize_t n = readlink(fname, symname, MY_MAX_PATH);

                if ((int)n == -1)
                    fprintf(stderr, "%.24s Tokudb No space when writing %" PRIu64 " bytes to fd=%d ", tstr, (uint64_t) len, fd);
                else {
		    tstr[n] = 0; // readlink doesn't append a NUL to the end of the buffer.
                    fprintf(stderr, "%.24s Tokudb No space when writing %" PRIu64 " bytes to %*s ", tstr, (uint64_t) len, (int) n, symname);
		}
                fprintf(stderr, "retry in %d second%s\n", toku_write_enospc_sleep, toku_write_enospc_sleep > 1 ? "s" : "");
                fflush(stderr);
            }
            sleep(toku_write_enospc_sleep);
            try_again = 1;
            __sync_fetch_and_sub(&toku_write_enospc_current, 1);
            break;
        }
    }
    default:
	break;
    }
    assert(try_again);
    errno = errno_write;
}

static ssize_t (*t_write)(int, const void *, size_t);
static ssize_t (*t_full_write)(int, const void *, size_t);
static ssize_t (*t_pwrite)(int, const void *, size_t, off_t);
static ssize_t (*t_full_pwrite)(int, const void *, size_t, off_t);
static FILE *  (*t_fdopen)(int, const char *);
static FILE *  (*t_fopen)(const char *, const char *);
static int     (*t_open)(const char *, int, int);
static int     (*t_fclose)(FILE *);
static ssize_t (*t_read)(int, void *, size_t);
static ssize_t (*t_pread)(int, void *, size_t, off_t);

void toku_set_func_write (ssize_t (*write_fun)(int, const void *, size_t)) {
    t_write = write_fun;
}

void toku_set_func_full_write (ssize_t (*write_fun)(int, const void *, size_t)) {
    t_full_write = write_fun;
}

void toku_set_func_pwrite (ssize_t (*pwrite_fun)(int, const void *, size_t, off_t)) {
    t_pwrite = pwrite_fun;
}

void toku_set_func_full_pwrite (ssize_t (*pwrite_fun)(int, const void *, size_t, off_t)) {
    t_full_pwrite = pwrite_fun;
}

void toku_set_func_fdopen(FILE * (*fdopen_fun)(int, const char *)) {
    t_fdopen = fdopen_fun;
}

void toku_set_func_fopen(FILE * (*fopen_fun)(const char *, const char *)) {
    t_fopen = fopen_fun;
}

void toku_set_func_open(int (*open_fun)(const char *, int, int)) {
    t_open = open_fun;
}

void toku_set_func_fclose(int (*fclose_fun)(FILE*)) {
    t_fclose = fclose_fun;
}

void toku_set_func_read (ssize_t (*read_fun)(int, void *, size_t)) {
    t_read = read_fun;
}

void toku_set_func_pread (ssize_t (*pread_fun)(int, void *, size_t, off_t)) {
    t_pread = pread_fun;
}

void
toku_os_full_write (int fd, const void *buf, size_t len) {
    const char *bp = (const char *) buf;
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
    const char *bp = (const char *) buf;
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
    const char *bp = (const char *) buf;
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
    const char *bp = (const char *) buf;
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
toku_os_close(int fd) {  // if EINTR, retry until success
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

ssize_t 
toku_os_read(int fd, void *buf, size_t count) {
    ssize_t r;
    if (t_read)
        r = t_read(fd, buf, count);
    else
        r = read(fd, buf, count);
    return r;
}

ssize_t
toku_os_pread (int fd, void *buf, size_t count, off_t offset) {
    ssize_t r;
    if (t_pread) {
	r = t_pread(fd, buf, count, offset);
    } else {
	r = pread(fd, buf, count, offset);
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

void toku_set_func_fsync(int (*fsync_function)(int)) {
    t_fsync = fsync_function;
}

// keep trying if fsync fails because of EINTR
static void file_fsync_internal (int fd, uint64_t *duration_p) {
    uint64_t tstart = toku_current_time_usec();
    int r = -1;
    while (r != 0) {
	if (t_fsync) {
	    r = t_fsync(fd);
        } else {
	    r = fsync(fd);
        }
	if (r) {
            assert(get_error_errno() == EINTR);
	}
    }
    __sync_fetch_and_add(&toku_fsync_count, 1);
    uint64_t duration = toku_current_time_usec() - tstart;
    __sync_fetch_and_add(&toku_fsync_time, duration);
    if (duration_p) {
        *duration_p = duration;
    }
}

void toku_file_fsync_without_accounting(int fd) {
    file_fsync_internal(fd, NULL);
}

void toku_fsync_dirfd_without_accounting(DIR *dir) {
    int fd = dirfd(dir);
    toku_file_fsync_without_accounting(fd);
}

int toku_fsync_dir_by_name_without_accounting(const char *dir_name) {
    int r = 0;
    DIR * dir = opendir(dir_name);
    if (!dir) {
        r = get_error_errno();
    } else {
        toku_fsync_dirfd_without_accounting(dir);
        r = closedir(dir);
        if (r != 0) {
            r = get_error_errno();
        }
    }
    return r;
}

// include fsync in scheduling accounting
void toku_file_fsync(int fd) {
    uint64_t duration;
    file_fsync_internal (fd, &duration);
    __sync_fetch_and_add(&sched_fsync_count, 1);
    __sync_fetch_and_add(&sched_fsync_time, duration);
}

// for real accounting
void toku_get_fsync_times(uint64_t *fsync_count, uint64_t *fsync_time) {
    *fsync_count = toku_fsync_count;
    *fsync_time = toku_fsync_time;
}

// for scheduling algorithm only
void toku_get_fsync_sched(uint64_t *fsync_count, uint64_t *fsync_time) {
    *fsync_count = sched_fsync_count;
    *fsync_time  = sched_fsync_time;
}

int toku_fsync_directory(const char *fname) {
    int result = 0;
    
    // extract dirname from fname
    const char *sp = strrchr(fname, '/');
    size_t len;
    char *dirname = NULL;
    if (sp) {
        resource_assert(sp >= fname);
        len = sp - fname + 1;
        MALLOC_N(len+1, dirname);
        if (dirname == NULL) {
            result = get_error_errno();
        } else {
            strncpy(dirname, fname, len);
            dirname[len] = 0;
        }
    } else {
        dirname = toku_strdup(".");
        if (dirname == NULL) {
            result = get_error_errno();
        }
    }

    if (result == 0) {
        result = toku_fsync_dir_by_name_without_accounting(dirname);
    }
    toku_free(dirname);
    return result;
}
