#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <syscall.h>
#include <sys/time.h>
#include <sys/resource.h>
#include <assert.h>
#include "portability.h"
#include "toku_os.h"

int
toku_os_getpid(void) {
    return getpid();
}

int
toku_os_gettid(void) {
    return syscall(__NR_gettid);
}

int
toku_os_get_number_processors(void) {
    return sysconf(_SC_NPROCESSORS_CONF);
}

int
toku_os_get_number_active_processors(void) {
    return sysconf(_SC_NPROCESSORS_ONLN);
}

int
toku_os_get_pagesize(void) {
    return sysconf(_SC_PAGESIZE);
}

uint64_t
toku_os_get_phys_memory_size(void) {
    uint64_t npages = sysconf(_SC_PHYS_PAGES);
    uint64_t pagesize = sysconf(_SC_PAGESIZE);
    return npages*pagesize;
}

int
toku_os_get_file_size(int fildes, int64_t *fsize) {
    struct stat sbuf;
    int r = fstat(fildes, &sbuf);
    if (r==0) {
        *fsize = sbuf.st_size;
    }
    return r;
}

int
toku_os_get_unique_file_id(int fildes, struct fileid *id) {
    struct stat statbuf;
    memset(id, 0, sizeof(*id));
    int r=fstat(fildes, &statbuf);
    if (r==0) {
        memset(id, 0, sizeof(*id));
        id->st_dev = statbuf.st_dev;
        id->st_ino = statbuf.st_ino;
    }
    return r;
}

int
toku_os_lock_file(char *name) {
    int r;
    int fd = open(name, O_RDWR|O_CREAT, S_IRUSR | S_IWUSR);
    if (fd>=0) {
        r = flock(fd, LOCK_EX | LOCK_NB);
        if (r!=0) {
            r = errno; //Save errno from flock.
            close(fd);
            fd = -1; //Disable fd.
            errno = r;
        }
    }
    return fd;
}

int
toku_os_unlock_file(int fildes) {
    int r = flock(fildes, LOCK_UN);
    if (r==0) r = close(fildes);
    return r;
}

int
toku_os_mkdir(const char *pathname, mode_t mode) {
    int r = mkdir(pathname, mode);
    return r;
}

int
toku_os_get_process_times(struct timeval *usertime, struct timeval *kerneltime) {
    int r;
    struct rusage rusage;
    r = getrusage(RUSAGE_SELF, &rusage);
    if (r == -1)
        return errno;
    if (usertime) 
        *usertime = rusage.ru_utime;
    if (kerneltime)
        *kerneltime = rusage.ru_stime;
    return 0;
}

int
toku_os_initialize_settings(int UU(verbosity)) {
    int r = 0;
    static int initialized = 0;
    assert(initialized==0);
    initialized=1;
    return r;
}

int
toku_os_get_max_rss(int64_t *maxrss) {
    char statusname[100];
    sprintf(statusname, "/proc/%d/status", getpid());
    FILE *f = fopen(statusname, "r");
    if (f == NULL)
        return errno;
    int r = ENOENT;
    char line[100];
    while (fgets(line, sizeof line, f)) {
        r = sscanf(line, "VmHWM:\t%lld kB\n", (long long *) maxrss);
        if (r == 1) { 
            *maxrss *= 1<<10;
            r = 0;
            break;
        }
    }
    fclose(f);
    return r;
}

int
toku_os_get_rss(int64_t *rss) {
    char statusname[100];
    sprintf(statusname, "/proc/%d/status", getpid());
    FILE *f = fopen(statusname, "r");
    if (f == NULL)
        return errno;
    int r = ENOENT;
    char line[100];
    while (fgets(line, sizeof line, f)) {
        r = sscanf(line, "VmRSS:\t%lld kB\n", (long long *) rss);
        if (r == 1) {
            *rss *= 1<<10;
            r = 0;
            break;
        }
    }
    fclose(f);
    return r;
}
