/* Copyright (c) 2010, 2016, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#ifndef X_PLATFORM_H
#define X_PLATFORM_H

#ifdef __cplusplus
extern "C" {
#endif



/*
 * Abstraction layer for lower level OS stuff needed
 */

/*
  Disable MY_ATTRIBUTE for Sun Studio and Visual Studio.
  Note that Sun Studio supports some __attribute__ variants,
  but not unused which we use quite a lot.
*/
#ifndef MY_ATTRIBUTE
#if defined(__GNUC__)
#  define MY_ATTRIBUTE(A) __attribute__(A)
#else
#  define MY_ATTRIBUTE(A)
#endif
#endif

/*
 * Platform independent functions
 */
#if defined(WIN32) || defined(WIN64)

#include <sys/locking.h>
#include <process.h>
#include <io.h>      /* open() */
#include <rpc/types.h>
#include <string.h>

#define NO_SIGPIPE
#define strdup _strdup

/* Reduce the amount of typing required when testing for any Windows */
#ifdef WIN
#error "WIN already defined"
#endif
#define WIN 1

#define bzero(p, n) memset(p, 0, n)

#ifndef inline
#define inline _inline
#endif

#define my_lrand48() rand()
#define my_srand48(x) srand(x)
#define my_drand48() ((double)rand()/RAND_MAX)

#define my_strdup(x) _strdup(x)

#define my_strtok(b, d, c) strtok_s(b, d, c)

#define my_strcasecmp(a, b) _stricmp(a, b) 

#ifndef STDERR
#define STDERR 2
#endif

typedef int mode_t;
typedef long ssize_t;

#define UINT64_MAX _UI64_MAX
#define INT64_MAX _I64_MAX
#define INT64_MIN _I64_MIN

#define _SHUT_RDWR SD_BOTH

static inline void thread_yield() { SwitchToThread(); }
#else /* defined (WIN32) || defined (WIN64) */

#include <unistd.h>
#include <fcntl.h>
#include <sched.h>
#include <string.h>
#include <strings.h>

#ifndef O_LARGEFILE
#define O_LARGEFILE 0
#endif

/* Reduce the amount of typing required when testing for not Windows */
#ifdef NWIN
#error "NWIN already defined!"
#endif
#define NWIN 1

#define my_lrand48() lrand48()
#define my_srand48(x) srand48(x)
#define my_drand48() drand48()

#define my_strdup(x) strdup(x)

#define my_strtok(b, d, c) strtok_r(b, d, c)

#define my_strcasecmp(a, b) strcasecmp(a, b)
 
#ifndef STDERR
#define STDERR 2
#endif

#define _SHUT_RDWR (SHUT_RD|SHUT_WR)
#define SOCKET_ERROR -1

static inline void thread_yield() { sched_yield(); }

#endif /* defined (WIN32) || defined (WIN64) */

#ifdef WIN
#define NEWLINE "\r\n"
#else
#define NEWLINE "\n"
#endif

#ifdef __cplusplus
}
#endif

#endif

