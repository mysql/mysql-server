/* ==== pthread_init.c ========================================================
 * Copyright (c) 1993, 1994 by Chris Provenzano, proven@mit.edu
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *  This product includes software developed by Chris Provenzano.
 * 4. The name of Chris Provenzano may not be used to endorse or promote 
 *	  products derived from this software without specific prior written
 *	  permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CHRIS PROVENZANO ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL CHRIS PROVENZANO BE LIABLE FOR ANY 
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR 
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER
 * CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT 
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF 
 * SUCH DAMAGE.
 *
 * Description : Pthread_init routine.
 *
 *  1.00 94/09/20 proven
 *      -Started coding this file.
 */

#ifndef lint
static const char rcsid[] = "$Id$";
#endif

#include <pthread.h>
#include <stdlib.h>
#include <string.h>

/* 
 * errno is declared here to prevent the linker from pulling in errno
 * from the C library (and whatever else is in that file). I also use
 * errno as the default location for error numbers for the initial thread
 * giving some backwards compatibility.
 */
#ifdef errno
#undef errno
#endif

#if !defined(M_UNIX)
int errno;
#else
extern int errno;
#endif

/* ==========================================================================
 * pthread_init()
 *
 * We use features of the C++ linker to make sure this function is called
 * before anything else is done in the program.  See init.cc.
 */
void pthread_init(void)
{
	struct machdep_pthread machdep_data = MACHDEP_PTHREAD_INIT;

	/* Only call this once */
	if (pthread_initial) {
		return;
	}

	pthread_pagesize = getpagesize();

	/* Initialize the first thread */
	if ((pthread_initial = (pthread_t)malloc(sizeof(struct pthread))) &&
	  (pthread_current_prio_queue = (struct pthread_prio_queue *)
	  malloc(sizeof(struct pthread_prio_queue)))) {
		memcpy(&(pthread_initial->machdep_data), &machdep_data, 
		  sizeof(machdep_data));
		memcpy(&pthread_initial->attr, &pthread_attr_default,
			   sizeof(pthread_attr_t));

		pthread_initial->pthread_priority = PTHREAD_DEFAULT_PRIORITY;
		pthread_initial->state = PS_RUNNING;

		pthread_queue_init(&(pthread_initial->join_queue));
		pthread_initial->specific_data = NULL;
		pthread_initial->specific_data_count = 0;
		pthread_initial->cleanup = NULL;
		pthread_initial->queue = NULL;
		pthread_initial->next = NULL;
		pthread_initial->flags = 0;
		pthread_initial->pll = NULL;
		pthread_initial->sll = NULL;

		/* PTHREADS spec says we start with cancellability on and deferred */
		SET_PF_CANCEL_STATE(pthread_initial, PTHREAD_CANCEL_ENABLE);
		SET_PF_CANCEL_TYPE(pthread_initial, PTHREAD_CANCEL_DEFERRED);


		/* Ugly errno hack */
		pthread_initial->error_p = &errno;
		pthread_initial->error = 0;

		pthread_prio_queue_init(pthread_current_prio_queue);
		pthread_link_list = pthread_initial;
		pthread_run = pthread_initial;

		uthread_sigmask = &(pthread_run->sigmask);

		/* XXX can I assume the mask and pending siganl sets are empty. */
		sigemptyset(&(pthread_initial->sigpending));
		sigemptyset(&(pthread_initial->sigmask));
		pthread_initial->sigcount = 0;

		/* Initialize the signal handler. */
		sig_init();

		/* Initialize the fd table. */
		fd_init();

		/* Start the scheduler */
		machdep_set_thread_timer(&(pthread_run->machdep_data));
#ifdef M_UNIX
		machdep_sys_init();
#endif
		return;
	}
	PANIC();
}
