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

#include "os0thread.h"

#if defined (__linux__) && HAVE_BACKTRACE && HAVE_BACKTRACE_SYMBOLS

#if HAVE_EXECINFO_H
#include <execinfo.h>
#endif

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#ifndef __USE_GNU
#define __USE_GNU
#endif
#ifndef _XOPEN_SOURCE
#define _XOPEN_SOURCE
#endif

/* Since kernel version 2.2 the undocumented parameter to the signal handler has been declared
obsolete in adherence with POSIX.1b. A more correct way to retrieve additional information is
to use the SA_SIGINFO option when setting the handler */
#undef USE_SIGCONTEXT

#ifndef USE_SIGCONTEXT
/* get REG_EIP / REG_RIP from ucontext.h */
#include <ucontext.h>

	#ifndef EIP
	#define EIP     14
	#endif

	#if (defined (__x86_64__))
		#ifndef REG_RIP
		#define REG_RIP REG_INDEX(rip) /* seems to be 16 */
		#endif
	#endif

#endif

#define OS_STACKTRACE_MAX_DEPTH 128

/***************************************************************//**
Prints stacktrace for this thread.
*/
void
os_stacktrace_print(
/*================*/
	int        sig_num,
	siginfo_t* info,
	void*      ucontext)
{
	void*      array[OS_STACKTRACE_MAX_DEPTH];
	char**     messages;
	int        size, i;
	void*      caller_address = NULL;

	/* Get the address at the time the signal was raised */
#if defined(__x86_64__)
	ucontext_t* uc = (ucontext_t*) ucontext;
	caller_address = (void*) uc->uc_mcontext.gregs[REG_RIP] ;
#elif defined(__hppa__)
	ucontext_t* uc = (ucontext_t*) ucontext;
	caller_address = (void*) uc->uc_mcontext.sc_iaoq[0] & ~0×3UL ;
#elif (defined (__ppc__)) || (defined (__powerpc__))
	ucontext_t* uc = (ucontext_t*) ucontext;
	caller_address = (void*) uc->uc_mcontext.regs->nip ;
#elif defined(__sparc__)
	struct sigcontext* sc = (struct sigcontext*) ucontext;
#if __WORDSIZE == 64
	caller_address = (void*) scp->sigc_regs.tpc ;
#else
	pnt = (void*) scp->si_regs.pc ;
#endif
#elif defined(__i386__)
	ucontext_t* uc = (ucontext_t*) ucontext;
	caller_address = (void*) uc->uc_mcontext.gregs[REG_EIP] ;
#else
	/* Unsupported return */
	return;
#endif

	fprintf(stderr, "InnoDB: signal %d (%s), address is %p from %p\n",
		sig_num, strsignal(sig_num), info->si_addr,
		(void *)caller_address);

	size = backtrace(array, OS_STACKTRACE_MAX_DEPTH);

	/* overwrite sigaction with caller's address */
	array[1] = caller_address;

	messages = backtrace_symbols(array, size);

	fprintf(stderr,
		"InnoDB: Stacktrace for Thread %lu \n",
		(ulong) os_thread_pf(os_thread_get_curr_id()));

	/* skip first stack frame (points here) */
	for (i = 1; i < size && messages != NULL; ++i)
	{
		fprintf(stderr, "InnoDB: [bt]: (%d) %s\n", i, messages[i]);
	}

	free(messages);
}

#endif /* __linux__ */
