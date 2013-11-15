/*****************************************************************************

Copyright (C) 2013 SkySQL Ab. All Rights Reserved.


This program is free software; you can redistribute it and/or modify it under
the terms of the GNU General Public License as published by the Free Software
Foundation; version 2 of the License.

This program is distributed in the hope that it will be useful, but WITHOUT
ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or FITNESS
FOR A PARTICULAR PURPOSE. See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License along with
this program; if not, write to the Free Software Foundation, Inc.,
51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA

*****************************************************************************/

#ifndef os0stacktrace_h
#define os0stacktrace_h

#ifdef __linux__
#if HAVE_EXECINFO_H
#include <execinfo.h>
#endif
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/***************************************************************//**
Prints stacktrace for this thread.
*/
void
os_stacktrace_print(
/*================*/
	int        sig_num,  /*!< in: signal number */
	siginfo_t* info,     /*!< in: signal information */
	void*      ucontext);/*!< in: signal context */

#endif /*  __linux__ */
#endif /* os0stacktrace.h */
