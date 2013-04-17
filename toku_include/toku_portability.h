/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
#ident "Copyright (c) 2007-2012 Tokutek Inc.  All rights reserved."
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#ifndef TOKU_PORTABILITY_H
#define TOKU_PORTABILITY_H

#include "config.h"

// Tokutek portability layer

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

#define DO_GCC_PRAGMA(x)      /* Nothing */

#if defined(__ICL)
#define __attribute__(x)      /* Nothing */
#endif

#include <malloc.h>
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

#define DO_GCC_PRAGMA(x)      /* Nothing */

#if defined(__ICC)
// Intel linux

#include <alloca.h>
#include <toku_stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <stdio.h>

#define static_assert(foo, bar)
#endif 

#if defined(__cplusplus) || defined(__cilkplusplus)
# define cast_to_typeof(v) (decltype(v))
#else
# define cast_to_typeof(v) (__typeof__(v))
#endif

#elif defined(__GNUC__)
// GCC linux

#define DO_GCC_PRAGMA(x) _Pragma (#x)

#include <toku_stdint.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/stat.h>
#include <stdio.h>
#if __FreeBSD__
#include <stdarg.h>
#endif
#if defined(HAVE_ALLOCA_H)
# include <alloca.h>
#endif
#if defined(__cplusplus) || defined(__cilkplusplus)
# include <type_traits>
#endif

#if defined(__cplusplus) || defined(__cilkplusplus)
# define cast_to_typeof(v) (decltype(v))
#else
# define cast_to_typeof(v) (__typeof__(v))
#endif

#else

#error Not ICC and not GNUC.  What compiler?

#endif

#if defined(__cplusplus) || defined(__cilkplusplus)
// decltype() here gives a reference-to-pointer instead of just a pointer,
// just use __typeof__
# define CAST_FROM_VOIDP(name, value) name = static_cast<__typeof__(name)>(value)
#else
# define CAST_FROM_VOIDP(name, value) name = cast_to_typeof(name) (value)
#endif

#ifndef TOKU_OFF_T_DEFINED
#define TOKU_OFF_T_DEFINED
typedef int64_t toku_off_t;
#endif

#include "toku_os.h"
#include "toku_htod.h"
#include "toku_assert.h"

#define UU(x) x __attribute__((__unused__))

#if defined(__cplusplus) || defined(__cilkplusplus)
extern "C" {
#endif

// Deprecated functions.
#if !defined(TOKU_ALLOW_DEPRECATED)
#   if defined(__ICL) || defined(__ICC) // Intel Compiler
#       pragma deprecated (creat, fstat, stat, getpid, syscall, sysconf, mkdir, strdup)
//#       pragma poison   off_t
//#       pragma poison   pthread_attr_t       pthread_t
//#       pragma poison   pthread_mutexattr_t  pthread_mutex_t
//#       pragma poison   pthread_condattr_t   pthread_cond_t
//#       pragma poison   pthread_rwlockattr_t pthread_rwlock_t
//#       pragma poison   timespec
#    ifndef DONT_DEPRECATE_WRITES
#       pragma poison   write                pwrite
#    endif
#    ifndef DONT_DEPRECATE_MALLOC
#       pragma deprecated (malloc, free, realloc)
#    endif
#    ifndef DONT_DEPRECATE_ERRNO
#       pragma deprecated (errno)
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
#    if defined(__FreeBSD__)
int syscall(int __sysno, ...)             __attribute__((__deprecated__));
#    else
long int syscall(long int __sysno, ...)             __attribute__((__deprecated__));
#    endif
// Sadly, dlmalloc needs sysconf, and on linux this causes trouble with -combine.  So let the warnings show up under windows only.
// long int sysconf(int)                   __attribute__((__deprecated__));
int      mkdir(const char *pathname, mode_t mode)   __attribute__((__deprecated__));
int      dup2(int fd, int fd2)                      __attribute__((__deprecated__));
int      _dup2(int fd, int fd2)                     __attribute__((__deprecated__));
// strdup is a macro in some libraries.
#undef strdup
#    if defined(__FreeBSD__)
char*    strdup(const char *)         __malloc_like __attribute__((__deprecated__));
#    else
char*    strdup(const char *)         __THROW __attribute_malloc__ __nonnull ((1)) __attribute__((__deprecated__));
#    endif
#undef __strdup
char*    __strdup(const char *)         __attribute__((__deprecated__));
#    ifndef DONT_DEPRECATE_WRITES
ssize_t  write(int, const void *, size_t)           __attribute__((__deprecated__));
ssize_t  pwrite(int, const void *, size_t, off_t)   __attribute__((__deprecated__));
#endif
#    ifndef DONT_DEPRECATE_MALLOC
#     if defined(__FreeBSD__)
extern void *malloc(size_t)                    __malloc_like __attribute__((__deprecated__));
extern void free(void*)                        __attribute__((__deprecated__));
extern void *realloc(void*, size_t)            __malloc_like __attribute__((__deprecated__));
#     else
extern void *malloc(size_t)                    __THROW __attribute__((__deprecated__));
extern void free(void*)                        __THROW __attribute__((__deprecated__));
extern void *realloc(void*, size_t)            __THROW __attribute__((__deprecated__));
#     endif
#    endif
#    ifndef DONT_DEPRECATE_ERRNO
//extern int errno __attribute__((__deprecated__));
#    endif
#pragma GCC poison u_int8_t
#pragma GCC poison u_int16_t
#pragma GCC poison u_int32_t
#pragma GCC poison u_int64_t
#pragma GCC poison BOOL
#pragma GCC poison FALSE
#pragma GCC poison TRUE
#   endif
#endif

#if defined(__cplusplus) || defined(__cilkplusplus)
};
#endif

void *os_malloc(size_t) __attribute__((__visibility__("default")));
void *os_realloc(void*,size_t) __attribute__((__visibility__("default")));
void os_free(void*) __attribute__((__visibility__("default")));

// full_pwrite and full_write performs a pwrite, and checks errors.  It doesn't return unless all the data was written. */
void toku_os_full_pwrite (int fd, const void *buf, size_t len, toku_off_t off) __attribute__((__visibility__("default")));
void toku_os_full_write (int fd, const void *buf, size_t len) __attribute__((__visibility__("default")));

// os_write returns 0 on success, otherwise an errno.
ssize_t toku_os_pwrite (int fd, const void *buf, size_t len, toku_off_t off) __attribute__((__visibility__("default")));
int toku_os_write (int fd, const void *buf, size_t len) __attribute__((__visibility__("default")));

// wrappers around file system calls
FILE * toku_os_fdopen(int fildes, const char *mode);    
FILE * toku_os_fopen(const char *filename, const char *mode);
int toku_os_open(const char *path, int oflag, int mode);
int toku_os_close(int fd);
int toku_os_fclose(FILE * stream);
ssize_t toku_os_read(int fd, void *buf, size_t count);
ssize_t toku_os_pread(int fd, void *buf, size_t count, off_t offset);

// wrapper around fsync
void toku_file_fsync_without_accounting(int fd);
void toku_file_fsync(int fd);
int toku_fsync_directory(const char *fname);

// get the number of fsync calls and the fsync times (total)
void toku_get_fsync_times(uint64_t *fsync_count, uint64_t *fsync_time);

// get the number of fsync calls and the fsync times for use by scheduler (subset of total)
void toku_get_fsync_sched(uint64_t *fsync_count, uint64_t *fsync_time);

void toku_set_func_fsync (int (*fsync_function)(int));
void toku_set_func_pwrite (ssize_t (*)(int, const void *, size_t, toku_off_t));
void toku_set_func_full_pwrite (ssize_t (*)(int, const void *, size_t, toku_off_t));
void toku_set_func_write (ssize_t (*)(int, const void *, size_t));
void toku_set_func_full_write (ssize_t (*)(int, const void *, size_t));
void toku_set_func_fdopen (FILE * (*)(int, const char *));
void toku_set_func_fopen (FILE * (*)(const char *, const char *));
void toku_set_func_open (int (*)(const char *, int, int));
void toku_set_func_fclose(int (*)(FILE*));
void toku_set_func_read(ssize_t (*)(int, void *, size_t));
void toku_set_func_pread (ssize_t (*)(int, void *, size_t, off_t));

int toku_portability_init(void);
void toku_portability_destroy(void);

#endif /* TOKU_PORTABILITY_H */
