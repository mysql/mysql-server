/* ==== dump_state.c ============================================================
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
 * $Id$
 *
 * Description : Bogus debugging output routines.
 *
 *  1.00 95/02/08 snl
 *      -Started coding this file.
 */

#include <pthread.h>
#include <stdio.h>

/* ==========================================================================
 * pthread_dump_state()
 *
 * Totally, totally bogus routine to dump the state of pthreads.
 */

void
pthread_dump_state()
{
  pthread_t thread;

  for (thread = pthread_link_list; thread; thread = thread->pll) {
    printf("Thread %lx", thread);
    if (thread == pthread_initial)
      printf("*");
    if (thread == pthread_run)
      printf("^");
    printf(" ");
    switch (thread->state) {
    case PS_RUNNING:		printf("RUNNING ");	break;
    case PS_MUTEX_WAIT:		printf("MUTEX_WAIT ");	break;
    case PS_COND_WAIT:		printf("COND_WAIT ");	break;
    case PS_FDLR_WAIT:		printf("FDLR_WAIT ");	break;
    case PS_FDLW_WAIT:		printf("FDLW_WAIT ");	break;
    case PS_FDR_WAIT:		printf("FDR_WAIT ");	break;
    case PS_FDW_WAIT:		printf("FDW_WAIT ");	break;
    case PS_SELECT_WAIT:	printf("SELECT ");	break;
    case PS_SLEEP_WAIT:		printf("SLEEP_WAIT ");	break;
    case PS_WAIT_WAIT:		printf("WAIT_WAIT ");	break;
    case PS_SIGWAIT:		printf("SIGWAIT ");	break;
    case PS_JOIN:		printf("JOIN ");	break;
    case PS_DEAD:		printf("DEAD ");	break;
    default:			printf("*UNKNOWN %d* ", thread->state);
							break;
    }
    switch (thread->attr.schedparam_policy) {
    case SCHED_RR:		printf("RR ");		break;
    case SCHED_IO:		printf("IO ");		break;
    case SCHED_FIFO:		printf("FIFO ");	break;
    case SCHED_OTHER:		printf("OTHER ");	break;
    default:			printf("*UNKNOWN %d* ",
				       thread->attr.schedparam_policy);
							break;
    }
  }
}
