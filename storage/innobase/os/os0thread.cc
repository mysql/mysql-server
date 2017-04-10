/*****************************************************************************

Copyright (c) 1995, 2016, Oracle and/or its affiliates. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin Street, Suite 500, Boston, MA 02110-1335 USA

*****************************************************************************/

/**************************************************//**
@file os/os0thread.cc
The interface to the operating system thread control primitives

Created 9/8/1995 Heikki Tuuri
*******************************************************/

#include "univ.i"

#include <thread>
#include <atomic>

/** We are prepared for a situation that we have this many threads waiting for
a semaphore inside InnoDB. innodb_init_params() sets the value. */
ulint srv_max_n_threads = 0;

/** Number of threads active. */
std::atomic_int os_thread_count;

/** Returns the thread identifier of current thread. Currently the thread
identifier in Unix is the thread handle itself.
@return current thread native handle */
os_thread_id_t
os_thread_get_curr_id()
{
#ifdef _WIN32
	return(reinterpret_cast<os_thread_id_t>(
			(UINT_PTR)::GetCurrentThreadId()));
#else
	return(::pthread_self());
#endif /* _WIN32 */
}

