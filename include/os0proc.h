/*****************************************************************************

Copyright (c) 1995, 2009, Innobase Oy. All Rights Reserved.

This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc., 59 Temple
Place, Suite 330, Boston, MA 02111-1307 USA

*****************************************************************************/

/**************************************************//**
@file include/os0proc.h
The interface to the operating system
process control primitives

Created 9/30/1995 Heikki Tuuri
*******************************************************/

#ifndef os0proc_h
#define os0proc_h

#include "univ.i"

#ifdef UNIV_LINUX
#include <sys/ipc.h>
#include <sys/shm.h>
#endif

typedef void*			os_process_t;
typedef unsigned long int	os_process_id_t;

extern ibool os_use_large_pages;
/* Large page size. This may be a boot-time option on some platforms */
extern ulint os_large_page_size;

/****************************************************************//**
Converts the current process id to a number. It is not guaranteed that the
number is unique. In Linux returns the 'process number' of the current
thread. That number is the same as one sees in 'top', for example. In Linux
the thread id is not the same as one sees in 'top'.
@return	process id as a number */
UNIV_INTERN
ulint
os_proc_get_number(void);
/*====================*/
/****************************************************************//**
Allocates large pages memory.
@return	allocated memory */
UNIV_INTERN
void*
os_mem_alloc_large(
/*===============*/
	ulint*	n,			/*!< in/out: number of bytes */
	ibool	populate);		/*!< in: virtual page preallocation */
/****************************************************************//**
Frees large pages memory. */
UNIV_INTERN
void
os_mem_free_large(
/*==============*/
	void	*ptr,			/*!< in: pointer returned by
					os_mem_alloc_large() */
	ulint	size);			/*!< in: size returned by
					os_mem_alloc_large() */

#ifndef UNIV_NONINL
#include "os0proc.ic"
#endif

#endif
