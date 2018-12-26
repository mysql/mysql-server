/* Copyright (c) 2015, 2017, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

/**
  @file include/my_thread_os_id.h
  Portable wrapper for gettid().
*/

#ifndef MY_THREAD_OS_ID_INCLUDED
#define MY_THREAD_OS_ID_INCLUDED

#include "my_global.h"              /* my_bool */

#ifndef _WIN32
#include <sys/types.h>              /* pid_t */
#include <sys/syscall.h>            /* SYS_gettid */
#include <unistd.h>                 /* syscall */
#include <pthread.h>                /* pthread_self */
#endif

#ifdef HAVE_PTHREAD_GETTHREADID_NP
#include <pthread_np.h>             /* pthread_getthreadid_np() */
#endif /* HAVE_PTHREAD_GETTHREADID_NP */

#ifdef HAVE_PTHREAD_THREADID_NP
#include <pthread.h>
#endif /* HAVE_PTHREAD_THREADID_NP */

C_MODE_START

typedef unsigned long long my_thread_os_id_t;

/**
  Return the operating system thread id.
  With Linux, threads have:
  - an internal id, @c pthread_self(), visible in process
  - an external id, @c gettid(), visible in the operating system,
    for example with perf in linux.
  This helper returns the underling operating system thread id.
*/
static inline my_thread_os_id_t my_thread_os_id()
{
#ifdef HAVE_PTHREAD_THREADID_NP
  /*
    macOS.

    Be careful to use this version first, and to not use SYS_gettid on macOS,
    as SYS_gettid has a different meaning compared to linux gettid().
  */
  uint64_t tid64;
  pthread_threadid_np(nullptr, &tid64);
  return (pid_t)tid64;
#else
#ifdef HAVE_SYS_GETTID
  /*
    Linux.
    See man gettid
    See GLIBC Bug 6399 - gettid() should have a wrapper
    https://sourceware.org/bugzilla/show_bug.cgi?id=6399
  */
  return syscall(SYS_gettid);
#else
#ifdef _WIN32
  /* Windows */
  return GetCurrentThreadId();
#else
#ifdef HAVE_PTHREAD_GETTHREADID_NP
  /* FreeBSD 10.2 */
  return pthread_getthreadid_np();
#else
#ifdef HAVE_INTEGER_PTHREAD_SELF
  /* Unknown platform, fallback. */
  return pthread_self();
#else
  /* Feature not available. */
  return 0;
#endif /* HAVE_INTEGER_PTHREAD_SELF */
#endif /* HAVE_PTHREAD_GETTHREADID_NP */
#endif /* _WIN32 */
#endif /* HAVE_SYS_GETTID */
#endif /* HAVE_SYS_THREAD_SELFID */
}

C_MODE_END

#endif /* MY_THREAD_OS_ID_INCLUDED */
