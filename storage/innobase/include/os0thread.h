/*****************************************************************************

Copyright (c) 1995, 2018, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License, version 2.0, as published by the
Free Software Foundation.

This program is also distributed with certain software (including but not
limited to OpenSSL) that is licensed under separate terms, as designated in a
particular file or component or in included license documentation. The authors
of MySQL hereby grant you an additional permission to link the program and
your derivative works with the separately licensed software that they have
included with MySQL.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License, version 2.0,
for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA

*****************************************************************************/

/** @file include/os0thread.h
 The interface to the operating system
 process and thread control primitives

 Created 9/8/1995 Heikki Tuuri
 *******************************************************/

#ifndef os0thread_h
#define os0thread_h

#include <thread>

/** Operating system thread native handle */
using os_thread_id_t = std::thread::native_handle_type;

/** Returns the thread identifier of current thread. Currently the thread
identifier in Unix is the thread handle itself.
@return current thread native handle */
os_thread_id_t os_thread_get_curr_id();

bool os_thread_set_priority(int priority);

void os_thread_set_priority(int priority, const char *thread_name);

/** Return the thread handle. The purpose of this function is to cast the
native handle to an integer type for consistency
@return the current thread ID cast to an uint64_t */
#define os_thread_handle() ((uint64_t)(os_thread_get_curr_id()))

/** Compares two thread ids for equality.
@param[in]	lhs	OS thread or thread id
@param[in]	rhs	OS thread or thread id
return true if equal */
#define os_thread_eq(lhs, rhs) ((lhs) == (rhs))

/** Advises the OS to give up remainder of the thread's time slice. */
#define os_thread_yield()      \
  do {                         \
    std::this_thread::yield(); \
  } while (false)

/** The thread sleeps at least the time given in microseconds.
@param[in]	usecs		time in microseconds */
#define os_thread_sleep(usecs)                                     \
  do {                                                             \
    std::this_thread::sleep_for(std::chrono::microseconds(usecs)); \
  } while (false)

#endif /* !os0thread_h */
