#ifndef TOKU_PORTABILITY_H
#define TOKU_PORTABILITY_H

// Tokutek portability layer

#if defined __cplusplus
extern "C" {
#endif

#if defined(_MSC_VER) || (defined(__INTEL_COMPILER) && defined(__ICL))

#define TOKU_WINDOWS 1
#define DEV_NULL_FILE "NUL"

#else

#define TOKU_WINDOWS 0
#define DEV_NULL_FILE "/dev/null"

#endif

#if TOKU_WINDOWS
// Windows

#if defined(__ICL)
#define __attribute__(x)      /* Nothing */
#endif

#include "stdint.h"
#include "inttypes.h"
#include <direct.h>
#include <sys/types.h>
#include "unistd.h"
#include "misc.h"
#include "toku_pthread.h"

#define UNUSED_WARNING(a) a=a /* To make up for missing attributes */

#elif defined(__INTEL_COMPILER)

#if defined(__ICC)
// Intel linux

#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>

#endif 

#elif defined(__GNUC__)
// GCC linux

#include <stdint.h>
#include <inttypes.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/time.h>

#else

#error Not ICC and not GNUC.  What compiler?

#endif

#include "toku_os.h"
#include "toku_htonl.h"

#define UU(x) x __attribute__((__unused__))

// Deprecated functions.
#if !defined(TOKU_ALLOW_DEPRECATED)
#   if defined(__ICL) //Windows Intel Compiler
#       pragma deprecated (creat, fstat, getpid, syscall, sysconf, mkdir, strdup)
#    ifndef DONT_DEPRECATE_MALLOC
#       pragma deprecated (malloc, free, realloc)
#    endif
#   else
int      creat()                        __attribute__((__deprecated__));
int      fstat()                        __attribute__((__deprecated__));
int      getpid(void)                   __attribute__((__deprecated__));
long int syscall(long int __sysno, ...) __attribute__((__deprecated__));
// Sadly, dlmalloc needs sysconf, and on linux this causes trouble with -combine.  So let the warnings show up under windows only.
// long int sysconf(int)                   __attribute__((__deprecated__));
int      mkdir()                        __attribute__((__deprecated__));
// strdup is a macro in some libraries.
#undef strdup
char*    strdup(const char *)           __attribute__((__deprecated__));
#undef __strdup
char*    __strdup(const char *)         __attribute__((__deprecated__));
#    ifndef DONT_DEPRECATE_MALLOC
void *malloc(size_t)                    __attribute__((__deprecated__));
void free(void*)                        __attribute__((__deprecated__));
void *realloc(void*, size_t)            __attribute__((__deprecated__));
#    endif
#   endif
#endif

void *os_malloc(size_t);
void *os_realloc(void*,size_t);
void  os_free(void*);
ssize_t toku_os_pwrite (int fd, const void *buf, size_t len, off_t off);
ssize_t toku_os_write (int fd, const void *buf, size_t len);

int toku_set_func_fsync (int (*fsync_function)(int));
int toku_set_func_malloc  (void *(*)(size_t));
int toku_set_func_realloc (void *(*)(void*,size_t));
int toku_set_func_free    (void (*)(void*));
int toku_set_func_pwrite (ssize_t (*pwrite_fun)(int, const void *, size_t, off_t));
int toku_set_func_write (ssize_t (*pwrite_fun)(int, const void *, size_t));

#if defined __cplusplus
};
#endif

#endif
