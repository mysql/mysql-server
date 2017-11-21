/* Copyright (c) 2000, 2017, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License as published by
  the Free Software Foundation; version 2 of the License.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software Foundation,
  51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA */

#ifndef COMPONENTS_SERVICES_MY_THREAD_BITS_H
#define COMPONENTS_SERVICES_MY_THREAD_BITS_H

/**
  @file mysql/components/services/my_thread_bits.h
  Types to make different thread packages compatible.
*/

#ifndef MYSQL_ABI_CHECK
#if defined(_WIN32)
#include <windows.h>
#else
#include <pthread.h>                // IWYU pragma: export
#include <sched.h>                  // IWYU pragma: export
#endif
#endif /* MYSQL_ABI_CHECK */

#ifdef _WIN32
typedef DWORD            my_thread_t;
typedef struct thread_attr
{
  DWORD dwStackSize;
  int detachstate;
} my_thread_attr_t;
#else
typedef pthread_t        my_thread_t;
typedef pthread_attr_t   my_thread_attr_t;
#endif

struct my_thread_handle
{
  my_thread_t thread{0};
#ifdef _WIN32
  HANDLE handle{INVALID_HANDLE_VALUE};
#endif
};

#endif /* COMPONENTS_SERVICES_MY_THREAD_BITS_H */
