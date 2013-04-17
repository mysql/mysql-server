#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/file.h>
#include <sys/stat.h>
#include <sys/resource.h>
#include "toku_portability.h"
#include "toku_os_types.h"
#include "toku_assert.h"

int
toku_portability_init(void) {
    return 0;
}

int
toku_portability_destroy(void) {
    return 0;
}

int
toku_os_getpid(void) {
    return getpid();
}

#if __FreeBSD__

int
toku_os_gettid(void) {
    long tid;
    int r = thr_self(&tid);
    assert(r == 0);
    return tid;
}

#endif

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
        flock_t flock;
	memset(&flock, 0, sizeof flock);
	flock.l_type = F_WRLCK;
	flock.l_whence = SEEK_SET;
	flock.l_start = flock.l_len = 0;
        r = fcntl(fd, F_SETLK, &flock);
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
    flock_t funlock;
    memset(&funlock, 0, sizeof funlock);
    funlock.l_type = F_UNLCK;
    funlock.l_whence = SEEK_SET;
    funlock.l_start = funlock.l_len = 0;
    int r = fcntl(fildes, F_SETLK, &funlock);
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
    verbosity = verbosity;
    return 0;
}

#if __linux__

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

#endif

int 
toku_os_is_absolute_name(const char* path) {
    return path[0] == '/';
}

int
toku_os_get_max_process_data_size(uint64_t *maxdata) {
    int r;
    struct rlimit rlimit;

    r = getrlimit(RLIMIT_DATA, &rlimit);
    if (r == 0) {
        uint64_t d;
        d = rlimit.rlim_max;
	// with the "right" macros defined, the rlimit is a 64 bit number on a
	// 32 bit system.  getrlimit returns 2**64-1 which is clearly wrong.

        // for 32 bit processes, we assume that 1/2 of the address space is
        // used for mapping the kernel.  this may be pessimistic.
        if (sizeof (void *) == 4 && d > (1ULL << 31))
            d = 1ULL << 31;
	*maxdata = d;
    } else
        r = errno;
    return r;
}

int
toku_stat(const char *name, toku_struct_stat *buf) {
    int r = stat(name, buf);
    return r;
}

int
toku_fstat(int fd, toku_struct_stat *buf) {
    int r = fstat(fd, buf);
    return r;
}
