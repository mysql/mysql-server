/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
/*
COPYING CONDITIONS NOTICE:

  This program is free software; you can redistribute it and/or modify
  it under the terms of version 2 of the GNU General Public License as
  published by the Free Software Foundation, and provided that the
  following conditions are met:

      * Redistributions of source code must retain this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below).

      * Redistributions in binary form must reproduce this COPYING
        CONDITIONS NOTICE, the COPYRIGHT NOTICE (below), the
        DISCLAIMER (below), the UNIVERSITY PATENT NOTICE (below), the
        PATENT MARKING NOTICE (below), and the PATENT RIGHTS
        GRANT (below) in the documentation and/or other materials
        provided with the distribution.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
  02110-1301, USA.

COPYRIGHT NOTICE:

  TokuFT, Tokutek Fractal Tree Indexing Library.
  Copyright (C) 2007-2013 Tokutek, Inc.

DISCLAIMER:

  This program is distributed in the hope that it will be useful, but
  WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
  General Public License for more details.

UNIVERSITY PATENT NOTICE:

  The technology is licensed by the Massachusetts Institute of
  Technology, Rutgers State University of New Jersey, and the Research
  Foundation of State University of New York at Stony Brook under
  United States of America Serial No. 11/760379 and to the patents
  and/or patent applications resulting from it.

PATENT MARKING NOTICE:

  This software is covered by US Patent No. 8,185,551.
  This software is covered by US Patent No. 8,489,638.

PATENT RIGHTS GRANT:

  "THIS IMPLEMENTATION" means the copyrightable works distributed by
  Tokutek as part of the Fractal Tree project.

  "PATENT CLAIMS" means the claims of patents that are owned or
  licensable by Tokutek, both currently or in the future; and that in
  the absence of this license would be infringed by THIS
  IMPLEMENTATION or by using or running THIS IMPLEMENTATION.

  "PATENT CHALLENGE" shall mean a challenge to the validity,
  patentability, enforceability and/or non-infringement of any of the
  PATENT CLAIMS or otherwise opposing any of the PATENT CLAIMS.

  Tokutek hereby grants to you, for the term and geographical scope of
  the PATENT CLAIMS, a non-exclusive, no-charge, royalty-free,
  irrevocable (except as stated in this section) patent license to
  make, have made, use, offer to sell, sell, import, transfer, and
  otherwise run, modify, and propagate the contents of THIS
  IMPLEMENTATION, where such license applies only to the PATENT
  CLAIMS.  This grant does not include claims that would be infringed
  only as a consequence of further modifications of THIS
  IMPLEMENTATION.  If you or your agent or licensee institute or order
  or agree to the institution of patent litigation against any entity
  (including a cross-claim or counterclaim in a lawsuit) alleging that
  THIS IMPLEMENTATION constitutes direct or contributory patent
  infringement, or inducement of patent infringement, then any rights
  granted to you under this License shall terminate as of the date
  such litigation is filed.  If you or your agent or exclusive
  licensee institute or order or agree to the institution of a PATENT
  CHALLENGE, then Tokutek may terminate any rights granted to you
  under this License.
*/

#ident "Copyright (c) 2007-2013 Tokutek Inc.  All rights reserved."
#ident "$Id$"

#include <portability/toku_config.h>

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <unistd.h>
#include <string.h>
#include <fcntl.h>
#include <errno.h>
#include <toku_assert.h>
#if defined(HAVE_MALLOC_H)
# include <malloc.h>
#elif defined(HAVE_SYS_MALLOC_H)
# include <sys/malloc.h>
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/file.h>
#if defined(HAVE_SYSCALL_H)
# include <syscall.h>
#endif
#if defined(HAVE_SYS_SYSCALL_H)
# include <sys/syscall.h>
#endif
#if defined(HAVE_SYS_SYSCTL_H)
# include <sys/sysctl.h>
#endif
#if defined(HAVE_PTHREAD_NP_H)
# include <pthread_np.h>
#endif
#include <inttypes.h>
#include <sys/time.h>
#if defined(HAVE_SYS_RESOURCE_H)
# include <sys/resource.h>
#endif
#include <sys/statvfs.h>
#include "toku_portability.h"
#include "toku_os.h"
#include "toku_time.h"
#include "memory.h"
#include <portability/toku_atomic.h>
#include <util/partitioned_counter.h>

int
toku_portability_init(void) {
    int r = toku_memory_startup();
    if (r == 0) {
        uint64_t hz;
        r = toku_os_get_processor_frequency(&hz); // get and cache freq
    }
    (void) toku_os_get_pagesize(); // get and cache pagesize
    return r;
}

void
toku_portability_destroy(void) {
    toku_memory_shutdown();
}

int
toku_os_getpid(void) {
    return getpid();
}

int
toku_os_gettid(void) {
#if defined(__NR_gettid)
    return syscall(__NR_gettid);
#elif defined(SYS_gettid)
    return syscall(SYS_gettid);
#elif defined(HAVE_PTHREAD_GETTHREADID_NP)
    return pthread_getthreadid_np();
#else
# error "no implementation of gettid available"
#endif
}

int
toku_os_get_number_processors(void) {
    return sysconf(_SC_NPROCESSORS_CONF);
}

int
toku_os_get_number_active_processors(void) {
    int n = sysconf(_SC_NPROCESSORS_ONLN);
#define DO_TOKU_NCPUS 1
#if DO_TOKU_NCPUS
    {
        char *toku_ncpus = getenv("TOKU_NCPUS");
        if (toku_ncpus) {
            int ncpus = atoi(toku_ncpus);
            if (ncpus < n)
                n = ncpus;
        }
    }
#endif
    return n;
}

int toku_cached_pagesize = 0;

int
toku_os_get_pagesize(void) {
    int pagesize = toku_cached_pagesize;
    if (pagesize == 0) {
        pagesize = sysconf(_SC_PAGESIZE);
        if (pagesize) {
            toku_cached_pagesize = pagesize;
        }
    }
    return pagesize;
}

uint64_t
toku_os_get_phys_memory_size(void) {
#if defined(_SC_PHYS_PAGES)
    uint64_t npages = sysconf(_SC_PHYS_PAGES);
    uint64_t pagesize = sysconf(_SC_PAGESIZE);
    return npages*pagesize;
#elif defined(HAVE_SYS_SYSCTL_H)
    uint64_t memsize;
    size_t len = sizeof memsize;
    sysctlbyname("hw.memsize", &memsize, &len, NULL, 0);
    return memsize;
#else
# error "cannot find _SC_PHYS_PAGES or sysctlbyname()"
#endif
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
toku_os_lock_file(const char *name) {
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
        return get_error_errno();
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

bool toku_os_is_absolute_name(const char* path) {
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
        r = get_error_errno();
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
        r = get_error_errno();
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
        r = get_error_errno();
    } else {
        uint64_t maxhz = 0;
        char *buf = NULL;
        size_t n = 0;
        while (getline(&buf, &n, fp) >= 0) {
            unsigned int cpu;
            sscanf(buf, "processor : %u", &cpu);
            unsigned int ma, mb;
            if (sscanf(buf, "cpu MHz : %u.%u", &ma, &mb) == 2) {
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

static int
toku_get_processor_frequency_sysctl(const char * const cmd, uint64_t *hzret) {
    int r = 0;
    FILE *fp = popen(cmd, "r");
    if (!fp) {
        r = EINVAL;  // popen doesn't return anything useful in errno,
                     // gotta pick something
    } else {
        r = fscanf(fp, "%" SCNu64, hzret);
        if (r != 1) {
            r = get_maybe_error_errno();
        } else {
            r = 0;
        }
        pclose(fp);
    }
    return r;
}

static uint64_t toku_cached_hz; // cache the value of hz so that we avoid opening files to compute it later

int 
toku_os_get_processor_frequency(uint64_t *hzret) {
    int r;
    if (toku_cached_hz) {
        *hzret = toku_cached_hz;
        r = 0;
    } else {
        r = toku_get_processor_frequency_sys(hzret);
        if (r != 0)
            r = toku_get_processor_frequency_cpuinfo(hzret);
        if (r != 0)
            r = toku_get_processor_frequency_sysctl("sysctl -n hw.cpufrequency", hzret);
        if (r != 0)
            r = toku_get_processor_frequency_sysctl("sysctl -n machdep.tsc_freq", hzret);
        if (r == 0)
            toku_cached_hz = *hzret;
    }
    return r;
}

int
toku_get_filesystem_sizes(const char *path, uint64_t *avail_size, uint64_t *free_size, uint64_t *total_size) {
    struct statvfs s;
    int r = statvfs(path, &s);
    if (r == -1) {
        r = get_error_errno();
    } else {
        // get the block size in bytes
        uint64_t bsize = s.f_frsize ? s.f_frsize : s.f_bsize;
        // convert blocks to bytes
        if (avail_size)
            *avail_size = (uint64_t) s.f_bavail * bsize;
        if (free_size) 
            *free_size = (uint64_t) s.f_bfree * bsize;
        if (total_size) 
            *total_size = (uint64_t) s.f_blocks * bsize;
    }
    return r;
}


int
toku_dup2(int fd, int fd2) {
    int r;
    r = dup2(fd, fd2);
    return r;
}


// Time
static       double seconds_per_clock = -1;

double tokutime_to_seconds(tokutime_t t) {
    // Convert tokutime to seconds.
    if (seconds_per_clock<0) {
	uint64_t hz;
	int r = toku_os_get_processor_frequency(&hz);
	assert(r==0);
	// There's a race condition here, but it doesn't really matter.  If two threads call tokutime_to_seconds
	// for the first time at the same time, then both will fetch the value and set the same value.
	seconds_per_clock = 1.0/hz;
    }
    return t*seconds_per_clock;
}

#include <toku_race_tools.h>
void __attribute__((constructor)) toku_portability_helgrind_ignore(void);
void
toku_portability_helgrind_ignore(void) {
    TOKU_VALGRIND_HG_DISABLE_CHECKING(&toku_cached_hz, sizeof toku_cached_hz);
    TOKU_VALGRIND_HG_DISABLE_CHECKING(&toku_cached_pagesize, sizeof toku_cached_pagesize);
}
