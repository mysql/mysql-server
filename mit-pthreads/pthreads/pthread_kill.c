/* ==== pthread_kill.c =======================================================
 * Copyright (c) 1994 by Chris Provenzano, proven@mit.edu
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
 * Description : pthread_kill function.
 *
 *  1.32 94/06/12 proven
 *      -Started coding this file.
 */

#ifndef lint
static const char rcsid[] = "$Id$";
#endif

#include <pthread.h>

/* Defined in sig.c, a linked list of threads currently
 * blocked in sigwait(): */
extern struct pthread * pthread_sigwait;


/* ==========================================================================
 * pthread_kill()
 */
int pthread_kill(struct pthread * pthread, int sig)
{
	struct pthread ** pthread_ptr;

	pthread_sched_prevent();

	/* Check who is the current owner of pthread */
/*	if (pthread->kthread != pthread_run->kthread) { */
	if (0) {
	} else {
  		if (pthread->state == PS_SIGWAIT) {
			if(sigismember(pthread->data.sigwait, sig)) {
			    for (pthread_ptr = &pthread_sigwait;
				 (*pthread_ptr);
				 pthread_ptr = &((*pthread_ptr)->next)) {
				if ((*pthread_ptr) == pthread) {

				    /* Take the thread out of the
				     * pthread_sigwait linked list: */
				    *pthread_ptr=(*pthread_ptr)->next;
			
				    *(int *)(pthread->ret) = sig;
				    pthread_sched_other_resume(pthread);
				    return(OK);
				}
			    }
			    /* A thread should not be in the state PS_SIGWAIT
			     * without being in the pthread_sigwait linked
			     * list: */
			    PANIC();
  			}
		}
		if (!sigismember(&pthread->sigpending,sig)) /* Added by monty */
		{
		  sigaddset(&(pthread->sigpending), sig);
		  pthread->sigcount++;
		}
	} 

	pthread_sched_resume();
	return(OK);
}
