#ifndef TOKU_PORTABILITY_H
#define TOKU_PORTABILITY_H

// Tokutek portability layer

#if defined(__cplusplus) || defined(__cilkplusplus)
extern "C" {
#endif

#if defined(_MSC_VER) || (defined(__INTEL_COMPILER) && defined(__ICL))

#define TOKU_WINDOWS 1
#define DEV_NULL_FILE "NUL"

# if defined(_WIN64)
#  define TOKU_WINDOWS_32 0
#  define TOKU_WINDOWS_64 1
# else
#  define TOKU_WINDOWS_32 1
#  define TOKU_WINDOWS_64 2
#endif


#else

#define TOKU_WINDOWS 0
#define TOKU_WINDOWS_32 0
#define TOKU_WINDOWS_64 0
#define DEV_NULL_FILE "/dev/null"

#endif

#if TOKU_WINDOWS
// Windows

#if defined(__ICL)
#define __attribute__(x)      /* Nothing */
#endif

#include "toku_stdint.h"

#ifndef TOKU_OFF_T_DEFINED
#define TOKU_OFF_T_DEFINED
typedef int64_t toku_off_t;
#endif

#include <direct.h>
#include <sys/types.h>
#include "unistd.h"
#include "misc.h"
#include "toku_pthread.h"

#define UNUSED_WARNING(a) a=a /* To make up for missing attributes */

#define cast_to_typeof(v)

#elif defined(__INTEL_COMPILER)

#if defined(__ICC)
// Intel linux

#include <toku_stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>

#endif 

#define cast_to_typeof(v) (__typeof__(v))
#elif defined(__GNUC__)
// GCC linux

#include <toku_stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#if __FreeBSD__
#include <stdarg.h>
#endif

#define cast_to_typeof(v) (__typeof__(v))
#else

#error Not ICC and not GNUC.  What compiler?

#endif

#ifndef TOKU_OFF_T_DEFINED
#define TOKU_OFF_T_DEFINED
typedef int64_t toku_off_t;
#endif

#include "toku_os.h"
#include "toku_htod.h"

#define UU(x) x __attribute__((__unused__))

// Deprecated functions.
#if !defined(TOKU_ALLOW_DEPRECATED)
#   if defined(__ICL) //Windows Intel Compiler
#       pragma deprecated (creat, fstat, stat, getpid, syscall, sysconf, mkdir, strdup)
#       pragma poison   off_t
#       pragma poison   pthread_attr_t       pthread_t
#       pragma poison   pthread_mutexattr_t  pthread_mutex_t
#       pragma poison   pthread_condattr_t   pthread_cond_t
#       pragma poison   pthread_rwlockattr_t pthread_rwlock_t
#       pragma poison   timespec
#    ifndef DONT_DEPRECATE_WRITES
#       pragma poison   write                pwrite
#    endif
#    ifndef DONT_DEPRECATE_MALLOC
#       pragma deprecated (malloc, free, realloc)
#    endif
#    ifndef TOKU_WINDOWS_ALLOW_DEPRECATED
#       pragma poison   dup2
#       pragma poison   _dup2
#    endif
#   else
int      creat(const char *pathname, mode_t mode)   __attribute__((__deprecated__));
int      fstat(int fd, struct stat *buf)            __attribute__((__deprecated__));
int      stat(const char *path, struct stat *buf)   __attribute__((__deprecated__));
int      getpid(void)                               __attribute__((__deprecated__));
long int syscall(long int __sysno, ...)             __attribute__((__deprecated__));
// Sadly, dlmalloc needs sysconf, and on linux this causes trouble with -combine.  So let the warnings show up under windows only.
// long int sysconf(int)                   __attribute__((__deprecated__));
int      mkdir(const char *pathname, mode_t mode)   __attribute__((__deprecated__));
int      dup2(int fd, int fd2)                      __attribute__((__deprecated__));
int      _dup2(int fd, int fd2)                     __attribute__((__deprecated__));
// strdup is a macro in some libraries.
#undef strdup
char*    strdup(const char *)           __attribute__((__deprecated__));
#undef __strdup
char*    __strdup(const char *)         __attribute__((__deprecated__));
#    ifndef DONT_DEPRECATE_WRITES
ssize_t  write(int, const void *, size_t)           __attribute__((__deprecated__));
ssize_t  pwrite(int, const void *, size_t, off_t)   __attribute__((__deprecated__));
#endif
#    ifndef DONT_DEPRECATE_MALLOC
extern void *malloc(size_t)                    __THROW __attribute__((__deprecated__)) ;
extern void free(void*)                        __THROW __attribute__((__deprecated__));
extern void *realloc(void*, size_t)            __THROW __attribute__((__deprecated__));
#    endif
#   endif
#endif

void *os_malloc(size_t);
void *os_realloc(void*,size_t);
void  os_free(void*);

// full_pwrite and full_write performs a pwrite, and checks errors.  It doesn't return unless all the data was written. */
void toku_os_full_pwrite (int fd, const void *buf, size_t len, toku_off_t off) __attribute__((__visibility__("default")));
void toku_os_full_write (int fd, const void *buf, size_t len) __attribute__((__visibility__("default")));

// os_write returns 0 on success, otherwise an errno.
int toku_os_pwrite (int fd, const void *buf, size_t len, toku_off_t off) __attribute__((__visibility__("default")));
int toku_os_write (int fd, const void *buf, size_t len) __attribute__((__visibility__("default")));

// wrapper around fsync
int toku_file_fsync_without_accounting(int fd);
int toku_file_fsync(int fd);

// get the number of fsync calls and the fsync times (total)
void toku_get_fsync_times(uint64_t *fsync_count, uint64_t *fsync_time);

// get the number of fsync calls and the fsync times for use by scheduler (subset of total)
void toku_get_fsync_sched(uint64_t *fsync_count, uint64_t *fsync_time);

// set a new fsync function (for debugging)
int toku_set_func_fsync (int (*fsync_function)(int));

int toku_set_func_malloc  (void *(*)(size_t));
int toku_set_func_realloc (void *(*)(void*,size_t));
int toku_set_func_free    (void (*)(void*));
int toku_set_func_pwrite (ssize_t (*pwrite_fun)(int, const void *, size_t, toku_off_t));
int toku_set_func_full_pwrite (ssize_t (*pwrite_fun)(int, const void *, size_t, toku_off_t));
int toku_set_func_write (ssize_t (*pwrite_fun)(int, const void *, size_t));
int toku_set_func_full_write (ssize_t (*pwrite_fun)(int, const void *, size_t));

int toku_portability_init    (void);
int toku_portability_destroy (void);

#if defined(__cplusplus) || defined(__cilkplusplus)
};
#endif

#endif
