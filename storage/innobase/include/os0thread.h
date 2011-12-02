/*****************************************************************************

Copyright (c) 1995, 2011, Oracle and/or its affiliates. All Rights Reserved.

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
@file include/os0thread.h
The interface to the operating system
process and thread control primitives

Created 9/8/1995 Heikki Tuuri
*******************************************************/

#ifndef os0thread_h
#define os0thread_h

#include "univ.i"

/* Maximum number of threads which can be created in the program;
this is also the size of the wait slot array for MySQL threads which
can wait inside InnoDB */

#define	OS_THREAD_MAX_N		srv_max_n_threads

/* Possible fixed priorities for threads */
#define OS_THREAD_PRIORITY_NONE		100
#define OS_THREAD_PRIORITY_BACKGROUND	1
#define OS_THREAD_PRIORITY_NORMAL	2
#define OS_THREAD_PRIORITY_ABOVE_NORMAL	3

#ifdef __WIN__
typedef void*			os_thread_t;
typedef DWORD			os_thread_id_t;	/*!< In Windows the thread id
						is an unsigned long int */
extern "C"  {
typedef LPTHREAD_START_ROUTINE	os_thread_func_t;
}

/** Macro for specifying a Windows thread start function. */
#define DECLARE_THREAD(func)	WINAPI func

/** Required to get around a build error on Windows. Even though our functions
are defined/declared as WINAPI f(LPVOID a); the compiler complains that they
are defined as: os_thread_ret_t (__cdecl*)(void*). Because our functions
don't access the arguments and don't return any value, we should be safe. */
#define os_thread_create(f,a,i)	\
	os_thread_create_func(reinterpret_cast<os_thread_func_t>(f), a, i)

#else

typedef pthread_t		os_thread_t;
typedef os_thread_t		os_thread_id_t;	/*!< In Unix we use the thread
						handle itself as the id of
						the thread */
extern "C"  { typedef void*	(*os_thread_func_t)(void*); }

/** Macro for specifying a POSIX thread start function. */
#define DECLARE_THREAD(func)	func
#define os_thread_create(f,a,i)	os_thread_create_func(f, a, i)

#endif /* __WIN__ */

/* Define a function pointer type to use in a typecast */
typedef void* (*os_posix_f_t) (void*);

#ifdef HAVE_PSI_INTERFACE
/* Define for performance schema registration key */
typedef unsigned int    mysql_pfs_key_t;
#endif

/***************************************************************//**
Compares two thread ids for equality.
@return	TRUE if equal */
UNIV_INTERN
ibool
os_thread_eq(
/*=========*/
	os_thread_id_t	a,	/*!< in: OS thread or thread id */
	os_thread_id_t	b);	/*!< in: OS thread or thread id */
/****************************************************************//**
Converts an OS thread id to a ulint. It is NOT guaranteed that the ulint is
unique for the thread though!
@return	thread identifier as a number */
UNIV_INTERN
ulint
os_thread_pf(
/*=========*/
	os_thread_id_t	a);	/*!< in: OS thread identifier */
/****************************************************************//**
Creates a new thread of execution. The execution starts from
the function given. The start function takes a void* parameter
and returns a ulint.
NOTE: We count the number of threads in os_thread_exit(). A created
thread should always use that to exit and not use return() to exit.
@return	handle to the thread */
UNIV_INTERN
os_thread_t
os_thread_create_func(
/*==================*/
	os_thread_func_t	func,		/*!< in: pointer to function
						from which to start */
	void*			arg,		/*!< in: argument to start
						function */
	os_thread_id_t*		thread_id);	/*!< out: id of the created
						thread, or NULL */

/*****************************************************************//**
Exits the current thread. */
UNIV_INTERN
void
os_thread_exit(
/*===========*/
	void*	exit_value)	/*!< in: exit value; in Windows this void*
				is cast as a DWORD */
	UNIV_COLD __attribute__((noreturn));
/*****************************************************************//**
Returns the thread identifier of current thread.
@return	current thread identifier */
UNIV_INTERN
os_thread_id_t
os_thread_get_curr_id(void);
/*========================*/
/*****************************************************************//**
Advises the os to give up remainder of the thread's time slice. */
UNIV_INTERN
void
os_thread_yield(void);
/*=================*/
/*****************************************************************//**
The thread sleeps at least the time given in microseconds. */
UNIV_INTERN
void
os_thread_sleep(
/*============*/
	ulint	tm);	/*!< in: time in microseconds */

#ifndef UNIV_NONINL
#include "os0thread.ic"
#endif

#endif
