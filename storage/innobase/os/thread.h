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
@file os/thread.h
The interface to the operating system
process and thread control primitives

Created 9/8/1995 Heikki Tuuri
*******************************************************/

/** NOTE: The functions in this file should only use functions from
other files in library. The code in this file is used to make a library for
external tools. */

#ifndef os_thread_h
#define os_thread_h

#include "univ.i"

#ifdef _WIN32
typedef DWORD			os_thread_id_t;	/*!< In Windows the thread id
						is an unsigned long int */
#else

typedef pthread_t		os_thread_id_t;	/*!< In Unix we use the thread
						handle itself as the id of
						the thread */
#endif /* _WIN32 */

/****************************************************************//**
Converts an OS thread id to a ulint. It is NOT guaranteed that the ulint is
unique for the thread though!
@return thread identifier as a number */
ulint
os_thread_pf(
/*=========*/
	os_thread_id_t	a);	/*!< in: OS thread identifier */
/*****************************************************************//**
Returns the thread identifier of current thread.
@return current thread identifier */
os_thread_id_t
os_thread_get_curr_id(void);
/*========================*/
/***************************************************************//**
Compares two thread ids for equality.
@return TRUE if equal */
ibool
os_thread_eq(
/*=========*/
	os_thread_id_t	a,	/*!< in: OS thread or thread id */
	os_thread_id_t	b);	/*!< in: OS thread or thread id */

#endif
