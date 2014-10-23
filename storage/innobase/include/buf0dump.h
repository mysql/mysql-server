/*****************************************************************************

Copyright (c) 2011, 2014, Oracle and/or its affiliates. All Rights Reserved.

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
@file buf/buf0dump.h
Implements a buffer pool dump/load.

Created April 08, 2011 Vasil Dimov
*******************************************************/

#ifndef buf0dump_h
#define buf0dump_h

#include "univ.i"

/*****************************************************************//**
Wakes up the buffer pool dump/load thread and instructs it to start
a dump. This function is called by MySQL code via buffer_pool_dump_now()
and it should return immediately because the whole MySQL is frozen during
its execution. */
void
buf_dump_start();
/*============*/

/*****************************************************************//**
Wakes up the buffer pool dump/load thread and instructs it to start
a load. This function is called by MySQL code via buffer_pool_load_now()
and it should return immediately because the whole MySQL is frozen during
its execution. */
void
buf_load_start();
/*============*/

/*****************************************************************//**
Aborts a currently running buffer pool load. This function is called by
MySQL code via buffer_pool_load_abort() and it should return immediately
because the whole MySQL is frozen during its execution. */
void
buf_load_abort();
/*============*/

/*****************************************************************//**
This is the main thread for buffer pool dump/load. It waits for an
event and when waked up either performs a dump or load and sleeps
again.
@return this function does not return, it calls os_thread_exit() */
extern "C"
os_thread_ret_t
DECLARE_THREAD(buf_dump_thread)(
/*============================*/
	void*	arg);				/*!< in: a dummy parameter
						required by os_thread_create */

#endif /* buf0dump_h */
