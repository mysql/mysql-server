/*****************************************************************************

Copyright (c) 1995, 2015, Oracle and/or its affiliates. All Rights Reserved.

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
@file os/thread.cc
The interface to the operating system
process and thread control primitives

Created 9/8/1995 Heikki Tuuri
*******************************************************/

/** NOTE: The functions in this file should only use functions from
other files in library. The code in this file is used to make a library for
external tools. */

#include "univ.i"
#include "os/thread.h"

/****************************************************************//**
Converts an OS thread id to a ulint. It is NOT guaranteed that the ulint is
unique for the thread though!
@return thread identifier as a number */
ulint
os_thread_pf(
/*=========*/
	os_thread_id_t	a)	/*!< in: OS thread identifier */
{
	return((ulint) a);
}

/*****************************************************************//**
Returns the thread identifier of current thread. Currently the thread
identifier in Unix is the thread handle itself. Note that in HP-UX
pthread_t is a struct of 3 fields.
@return current thread identifier */
os_thread_id_t
os_thread_get_curr_id(void)
/*=======================*/
{
#ifdef _WIN32
	return(GetCurrentThreadId());
#else
	return(pthread_self());
#endif
}

/***************************************************************//**
Compares two thread ids for equality.
@return TRUE if equal */
ibool
os_thread_eq(
/*=========*/
	os_thread_id_t	a,	/*!< in: OS thread or thread id */
	os_thread_id_t	b)	/*!< in: OS thread or thread id */
{
#ifdef _WIN32
	if (a == b) {
		return(TRUE);
	}

	return(FALSE);
#else
	if (pthread_equal(a, b)) {
		return(TRUE);
	}

	return(FALSE);
#endif
}
