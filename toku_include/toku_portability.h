/* -*- mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*- */
// vim: ft=cpp:expandtab:ts=8:sw=4:softtabstop=4:
#ident "$Id$"
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

  TokuDB, Tokutek Fractal Tree Indexing Library.
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
#ident "The technology is licensed by the Massachusetts Institute of Technology, Rutgers State University of New Jersey, and the Research Foundation of State University of New York at Stony Brook under United States of America Serial No. 11/760379 and to the patents and/or patent applications resulting from it."
#ifndef TOKU_PORTABILITY_H
#define TOKU_PORTABILITY_H

#include "toku_config.h"

// Tokutek portability layer

#if defined(__clang__)
#  define constexpr_static_assert(a, b)
#else
#  define constexpr_static_assert(a, b) static_assert(a, b)
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

// include here, before they get deprecated
#include <toku_atomic.h>

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

#if defined(__cplusplus)
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
#if defined(__cplusplus)
# include <type_traits>
#endif

#if defined(__cplusplus)
# define cast_to_typeof(v) (decltype(v))
#else
# define cast_to_typeof(v) (__typeof__(v))
#endif

#else

#error Not ICC and not GNUC.  What compiler?

#endif

// Define some constants for Yama in case the build-machine's software is too old.
#if !defined(HAVE_PR_SET_PTRACER)
/*
 * Set specific pid that is allowed to ptrace the current task.
 * A value of 0 mean "no process".
 */
// Well defined ("Yama" in ascii)
#define PR_SET_PTRACER 0x59616d61
#endif
#if !defined(HAVE_PR_SET_PTRACER_ANY)
#define PR_SET_PTRACER_ANY ((unsigned long)-1)
#endif

#if defined(__cplusplus)
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
#include "toku_crash.h"

#define UU(x) x __attribute__((__unused__))

#if defined(__cplusplus)
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
#    if defined(__FreeBSD__) || defined(__APPLE__)
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
#    elif defined(__APPLE__)
char*    strdup(const char *)         __attribute__((__deprecated__));
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
#     elif defined(__APPLE__)
extern void *malloc(size_t)                    __attribute__((__deprecated__));
extern void free(void*)                        __attribute__((__deprecated__));
extern void *realloc(void*, size_t)            __attribute__((__deprecated__));
#     else
extern void *malloc(size_t)                    __THROW __attribute__((__deprecated__));
extern void free(void*)                        __THROW __attribute__((__deprecated__));
extern void *realloc(void*, size_t)            __THROW __attribute__((__deprecated__));
#     endif
#    endif
#    ifndef DONT_DEPRECATE_ERRNO
//extern int errno __attribute__((__deprecated__));
#    endif
#if !defined(__APPLE__)
// Darwin headers use these types, we should not poison them
# pragma GCC poison u_int8_t
# pragma GCC poison u_int16_t
# pragma GCC poison u_int32_t
# pragma GCC poison u_int64_t
# pragma GCC poison BOOL
# pragma GCC poison FALSE
# pragma GCC poison TRUE
#endif
#pragma GCC poison __sync_fetch_and_add
#pragma GCC poison __sync_fetch_and_sub
#pragma GCC poison __sync_fetch_and_or
#pragma GCC poison __sync_fetch_and_and
#pragma GCC poison __sync_fetch_and_xor
#pragma GCC poison __sync_fetch_and_nand
#pragma GCC poison __sync_add_and_fetch
#pragma GCC poison __sync_sub_and_fetch
#pragma GCC poison __sync_or_and_fetch
#pragma GCC poison __sync_and_and_fetch
#pragma GCC poison __sync_xor_and_fetch
#pragma GCC poison __sync_nand_and_fetch
#pragma GCC poison __sync_bool_compare_and_swap
#pragma GCC poison __sync_val_compare_and_swap
#pragma GCC poison __sync_synchronize
#pragma GCC poison __sync_lock_test_and_set
#pragma GCC poison __sync_release
#   endif
#endif

#if defined(__cplusplus)
};
#endif

void *os_malloc(size_t) __attribute__((__visibility__("default")));
// Effect: See man malloc(2)

void *os_malloc_aligned(size_t /*alignment*/, size_t /*size*/) __attribute__((__visibility__("default")));
// Effect: Perform a malloc(size) with the additional property that the returned pointer is a multiple of ALIGNMENT.
// Requires: alignment is a power of two.


void *os_realloc(void*,size_t) __attribute__((__visibility__("default")));
// Effect: See man realloc(2)

void *os_realloc_aligned(size_t/*alignment*/, void*,size_t) __attribute__((__visibility__("default")));
// Effect: Perform a realloc(p, size) with the additional property that the returned pointer is a multiple of ALIGNMENT.
// Requires: alignment is a power of two.

void os_free(void*) __attribute__((__visibility__("default")));
// Effect: See man free(2)

size_t os_malloc_usable_size(const void *p) __attribute__((__visibility__("default")));
// Effect: Return an estimate of the usable size inside a pointer.  If this function is not defined the memory.cc will
//  look for the jemalloc, libc, or darwin versions of the function for computing memory footprint.

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
int toku_os_open_direct(const char *path, int oflag, int mode);
int toku_os_close(int fd);
int toku_os_fclose(FILE * stream);
ssize_t toku_os_read(int fd, void *buf, size_t count);
ssize_t toku_os_pread(int fd, void *buf, size_t count, off_t offset);
void toku_os_recursive_delete(const char *path);

// wrapper around fsync
void toku_file_fsync_without_accounting(int fd);
void toku_file_fsync(int fd);
int toku_fsync_directory(const char *fname);

// get the number of fsync calls and the fsync times (total)
void toku_get_fsync_times(uint64_t *fsync_count, uint64_t *fsync_time, uint64_t *long_fsync_threshold, uint64_t *long_fsync_count, uint64_t *long_fsync_time);

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

static inline uint64_t roundup_to_multiple(uint64_t alignment, uint64_t v)
// Effect: Return X, where X the smallest multiple of ALIGNMENT such that X>=V.
// Requires: ALIGNMENT is a power of two
{
    assert(0==(alignment&(alignment-1)));  // alignment must be a power of two
    uint64_t result = (v+alignment-1)&~(alignment-1);
    assert(result>=v);                     // The result is >=V.
    assert(result%alignment==0);           // The result is a multiple of alignment.
    assert(result<v+alignment);            // The result is the smallest such multiple of alignment.
    return result;
}
    

#endif /* TOKU_PORTABILITY_H */
