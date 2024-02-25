/* Copyright (c) 2010, 2023, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef X_PLATFORM_H
#define X_PLATFORM_H

/*
 * Abstraction layer for lower level OS stuff needed
 */

#ifdef XCOM_STANDALONE
/*
  Disable MY_ATTRIBUTE for Sun Studio and Visual Studio.
  Note that Sun Studio supports some __attribute__ variants,
  but not unused which we use quite a lot.
*/
#ifndef MY_ATTRIBUTE
#if defined(__GNUC__)
#define MY_ATTRIBUTE(A) __attribute__(A)
#else
#define MY_ATTRIBUTE(A)
#endif
#endif
#endif

/*
 * Platform independent functions
 */
#if defined(_WIN32)

#include <io.h> /* open() */
#include <process.h>
#include <rpc/types.h>
#include <string.h>
#include <sys/locking.h>

#define NO_SIGPIPE
#define strdup _strdup

#define bzero(p, n) memset(p, 0, n)

#ifndef __cplusplus
#ifndef inline
#define inline _inline
#endif
#endif
#define xcom_close(fd) _close(fd)
#define xcom_fclose(f) fclose(f)
#define xcom_chdir(s) _chdir(s)
#define xcom_dup(s) _dup(s)
#define xcom_dup2(s, t) _dup2(s, t)
#define xcom_lseek(fd, of, h) _lseek(fd, of, h)
#define xcom_open(n, f, m) _open(n, f, m)
#define xcom_fopen(n, m) fopen(n, m)
#define xcom_fstat(f, b) _fstat(f, b)

#define XCOM_F_TLOCK _LK_NBLCK /* Lock without retry in _locking() */
#define xcom_lockf(fd, f, sz) _locking(fd, f, sz)

#define xcom_lrand48() rand()
#define xcom_srand48(x) srand(x)
#define xcom_drand48() ((double)rand() / RAND_MAX)

#define xcom_write(fd, buf, l) _write(fd, buf, (unsigned int)l)
#define xcom_read(fd, buf, c) _read(fd, buf, (unsigned int)c)

#define xcom_mktemp(s) _mktemp(s)

/* WARNING: snprintf() != _snprintf() */
/* #define xcom_snprintf(argv) _snprintf(argv) */
#define xcom_strdup(x) _strdup(x)

#define xcom_strtok(b, d, c) strtok_s(b, d, c)

#define xcom_strcasecmp(a, b) _stricmp(a, b)

#define xcom_execv _execv

/** Posix states that the optval argument should be (const) void*, but
    on Windows it is (const) char* (See
    http://msdn.microsoft.com/en-us/library/windows/desktop/ms740476(v=vs.85).aspx
    and since we normally pass in int* a cast is required on Windows.

    With this typedef we can call xsockopt functions uniformly across
    platorms. */
typedef char *xcom_sockoptptr_t;

typedef int mode_t;
typedef SSIZE_T ssize_t;

#ifndef HAVE_STDINT_H
#ifndef UINT64_MAX
#define UINT64_MAX _UI64_MAX
#endif
#ifndef INT64_MAX
#define INT64_MAX _I64_MAX
#endif
#ifndef INT64_MIN
#define INT64_MIN _I64_MIN
#endif
#ifndef UINT32_MAX
#define UINT32_MAX _UI32_MAX
#endif
#ifndef INT32_MAX
#define INT32_MAX _I32_MAX
#endif
#ifndef INT32_MIN
#define INT32_MIN _I32_MIN
#endif
#ifndef UINT16_MAX
#define UINT16_MAX _UI16_MAX
#endif
#ifndef INT16_MAX
#define INT16_MAX _I16_MAX
#endif
#ifndef INT16_MIN
#define INT16_MIN _I16_MIN
#endif
#endif

#ifndef PRIu64
#ifdef _LP64
#define PRIu64 "lu"
#else /* _ILP32 */
#define PRIu64 "llu"
#endif
#endif

#ifndef PRIu32
#define PRIu32 "u"
#endif

#define XCOM_O_CREAT _O_CREAT
#define XCOM_O_WRONLY _O_WRONLY
#define XCOM_O_APPEND _O_APPEND
#define XCOM_O_LARGEFILE 0
#define XCOM_O_EXCL _O_EXCL
#define XCOM_O_RDWR _O_RDWR
#define XCOM_O_RDONLY _O_RDONLY
#define XCOM_O_RSYNC 0
#define XCOM_O_BINARY _O_BINARY

/* Only global flags can be set on Windows */
#define XCOM_S_IRUSR _S_IREAD
#define XCOM_S_IWUSR _S_IWRITE
#define XCOM_S_IXUSR 0
#define XCOM_S_IWRXU XCOM_S_IRUSR | XCOM_S_IWUSR | XCOM_S_IXUSR
#define XCOM_S_IRGRP 0
#define XCOM_S_IWGRP 0
#define XCOM_S_IROTH 0
#define XCOM_S_IWOTH 0

#define XCOM_FILE_WRITE_MODE "w"

/**
 * Process exitcode macros.
 *
 * EXCEPTION_ or STATUS_ exits on windows start with 0xCxxxxxxx. The below
 * attempts to use 2nd highest bit to determine if normal or error exit.
 *
 * These macros are normally found on *ix systems, and is used to mimic *ix.
 */
#define WEXITSTATUS(s) (s)
#define WIFEXITED(s) ((s) == -1 ? 1 : ((s)&0x40000000) == 0)
#define WIFSIGNALED(s) ((s) == -1 ? 0 : ((s)&0x40000000) != 0)
#define WTERMSIG(s) ((s) == -1 ? 0 : ((s)&0x3FFFFFFF))
#define WCOREDUMP(s) (((s) == CORE_EXIT_VALUE_WIN32) ? TRUE : FALSE)

typedef int mode_t;
typedef SSIZE_T ssize_t;

#define XCOM_CLRSYSERR (errno = 0, WSASetLastError(0))
#define XCOM_ISSYSERR (errno != 0 || WSAGetLastError() != 0)
#define XCOM_SYSERRNUM (WSAGetLastError() ? WSAGetLastError() : (errno))
#define XCOM_SYSERRSTR \
  (WSAGetLastError() ? g_strerror(WSAGetLastError()) : g_strerror(errno))
#define xcom_gmtime_r(time, res) gmtime_s(res, time)
#define xcom_localtime_r(time, res) localtime_s(res, time)

static inline void thread_yield() { SwitchToThread(); }
#else /* defined (_WIN32)*/

#include <fcntl.h>
#include <sched.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

/* TODO: Decide in one prefix for system-independent names (xcom or
   xcom). Or perhaps really something generic that doesn't depend on
   the current product name */
#define xcom_close(fd) close(fd)
#define xcom_fclose(f) fclose(f)
#define xcom_chdir(s) chdir(s)
#define xcom_dup(s) dup(s)
#define xcom_dup2(s, t) dup2(s, t)
#define xcom_lseek(fd, of, h) lseek(fd, of, h)
#define xcom_open(n, f, m) open(n, f, m)
#define xcom_fopen(n, m) fopen(n, m)
#define xcom_fstat(f, b) fstat(f, b)

#define XCOM_F_TLOCK F_TLOCK
#define xcom_lockf(fd, f, sz) lockf(fd, f, sz)

#define xcom_lrand48() lrand48()
#define xcom_srand48(x) srand48(x)
#define xcom_drand48() drand48()

#define xcom_write(fd, buf, l) write(fd, buf, l)
#define xcom_read(fd, buf, c) read(fd, buf, c)
#define xcom_execv execv

#define xcom_mktemp(s) mktemp(s)

/* WARNING: snprintf() != _snprintf() */
/* #define xcom_snprintf(argv) snprintf(argv) */
#define xcom_strdup(x) strdup(x)
#if defined(_WIN32)
#define xcom_strtok(b, d, c) strtok_s(b, d, c)
#else
#define xcom_strtok(b, d, c) strtok_r(b, d, c)
#endif

#define xcom_strcasecmp(a, b) strcasecmp(a, b)

/** Posix states that the optval argument should be (const) void*, but
    on Windows it is (const) char* and since we normally pass in int*
    a cast is required on Windows.

    With this typedef we can call xsockopt functions uniformly across
    platorms, allbeit with an unnecessary cast to void* on *nix. */
typedef void *xcom_sockoptptr_t;

#define XCOM_O_CREAT O_CREAT
#define XCOM_O_WRONLY O_WRONLY
#define XCOM_O_APPEND O_APPEND
#define XCOM_O_LARGEFILE O_LARGEFILE
#define XCOM_O_EXCL O_EXCL
#define XCOM_O_RDWR O_RDWR
#define XCOM_O_RDONLY O_RDONLY
#ifdef __APPLE__
/* OSX does not define O_RSYNC, use O_DSYNC instead */
#define XCOM_O_RSYNC O_DSYNC
#else
#define XCOM_O_RSYNC O_RSYNC
#endif
#define XCOM_O_BINARY 0 /** Empty define - applicable WIN32 only */

#define XCOM_S_IRUSR S_IRUSR
#define XCOM_S_IWUSR S_IWUSR
#define XCOM_S_IXUSR S_IXUSR
#define XCOM_S_IWRXU XCOM_S_IRUSR | XCOM_S_IWUSR | XCOM_S_IXUSR
#define XCOM_S_IRGRP S_IRGRP
#define XCOM_S_IWGRP S_IWGRP
#define XCOM_S_IROTH S_IROTH
#define XCOM_S_IWOTH S_IWOTH

#define XCOM_FILE_WRITE_MODE "wF"

#define SOCKET_ERROR -1

#define XCOM_CLRSYSERR errno = 0
#define XCOM_ISSYSERR (errno != 0)
#define XCOM_SYSERRNUM ((errno) + 0)
#define XCOM_SYSERRSTR g_strerror(errno)

#define xcom_gmtime_r(time, res) gmtime_r(time, res)
#define xcom_localtime_r(time, res) localtime_r(time, res)

#define thread_yield sched_yield

#endif /* defined (_WIN32) */

#ifdef _WIN32
#define NEWLINE "\r\n"
#else
#define NEWLINE "\n"
#endif

#ifdef _WIN32
#define xcom_buf char
#else
#define xcom_buf void
#endif

#endif
