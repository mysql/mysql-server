#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <toku_assert.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#include <syscall.h>
#include <sys/time.h>
#include <sys/resource.h>
#include "toku_portability.h"
#include "toku_os.h"
#include <malloc.h>

static int
toku_mallopt_init(void) {
    int r = mallopt(M_MMAP_THRESHOLD, 1024*64); // 64K and larger should be malloced with mmap().
    return r;
}

int
toku_portability_init(void) {
    int r = 0;
    if (r==0) {
        int success = toku_mallopt_init(); //mallopt returns 1 on success, 0 on error
        assert(success);
    }
    return r;
}

int
toku_portability_destroy(void) {
    int r = 0;
    return r;
}

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
    toku_struct_stat sbuf;
    int r = fstat(fildes, &sbuf);
    if (r==0) {
        *fsize = sbuf.st_size;
    }
    return r;
}

int
toku_os_get_unique_file_id(int fildes, struct fileid *id) {
    toku_struct_stat statbuf;
    memset(id, 0, sizeof(*id));
    int r=fstat(fildes, &statbuf);
    if (r==0) {
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

static int
toku_get_processor_frequency_sys(uint64_t *hzret) {
    int r;
    FILE *fp = fopen("/sys/devices/system/cpu/cpu0/cpufreq/cpuinfo_max_freq", "r");
    if (!fp) 
        r = errno;
    else {
        unsigned int khz = 0;
        if (fscanf(fp, "%u", &khz) == 1) {
            *hzret = khz * 1000ULL;
            r = 0;
        } else
            r = ENOENT;
        fclose(fp);
    }
    return r;
}

static int
toku_get_processor_frequency_cpuinfo(uint64_t *hzret) {
    int r;
    FILE *fp = fopen("/proc/cpuinfo", "r");
    if (!fp) {
        r = errno;
    } else {
        uint64_t maxhz = 0;
        char *buf = NULL;
        size_t n = 0;
        while (getline(&buf, &n, fp) >= 0) {
            unsigned int cpu;
            sscanf(buf, "processor : %u", &cpu);
            unsigned int ma, mb;
            if (sscanf(buf, "cpu MHz : %d.%d", &ma, &mb) == 2) {
                uint64_t hz = ma * 1000000ULL + mb * 1000ULL;
                if (hz > maxhz)
                    maxhz = hz;
            }
        }
        if (buf)
            free(buf);
        fclose(fp);
        *hzret = maxhz;
        r = maxhz == 0 ? ENOENT : 0;;
    }
    return r;
}

int
toku_os_get_processor_frequency(uint64_t *hzret) {
    int r;
    r = toku_get_processor_frequency_sys(hzret);
    if (r != 0)
        r = toku_get_processor_frequency_cpuinfo(hzret);
    return r;
}

int
toku_dup2(int fd, int fd2) {
    int r;
    r = dup2(fd, fd2);
    return r;
}

#if __GNUC__ && __i386__

// workaround for a gcc 4.1.2 bug on 32 bit platforms.
uint64_t toku_sync_fetch_and_add_uint64(volatile uint64_t *a, uint64_t b) __attribute__((noinline));

uint64_t toku_sync_fetch_and_add_uint64(volatile uint64_t *a, uint64_t b) {
    return __sync_fetch_and_add(a, b);
}

#endif
