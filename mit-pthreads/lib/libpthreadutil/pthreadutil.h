/* ==== pthread_tad.h ========================================================
 * Copyright (c) 1995 by Chris Provenzano, proven@athena.mit.edu
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
 *	This product includes software developed by Chris Provenzano,
 *	and its contributors.
 * 4. Neither the name of Chris Provenzano, nor the names of
 *	  its contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY CHRIS PROVENZANO, AND CONTRIBUTORS
 * ``AS IS'' AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED
 * TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE
 * LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR
 * CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
 * SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
 * INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN
 * CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE
 * POSSIBILITY OF SUCH DAMAGE.
 *
 */

#include <pthread.h>
#include <sys/cdefs.h>

typedef struct pthread_tad_t {
	pthread_mutex_t 		mutex;
	pthread_cond_t 			cond;
	unsigned int 			count_current;
	unsigned int 			count_max;
	void *					arg;
	void *					(*routine)();
} pthread_tad_t;

typedef struct pthread_atexit {
	struct pthread_atexit	* next;
	struct pthread_atexit	* prev;
	void 					  (*rtn)(void *);
	void					* arg;
} * pthread_atexit_t;

/*
 * New functions
 */

__BEGIN_DECLS

int	pthread_tad_count		__P_((pthread_tad_t *));
int pthread_tad_create		__P_((pthread_tad_t *, pthread_t *, pthread_attr_t *,
							 void *(*routine)(), void *));
int	pthread_tad_wait		__P_((pthread_tad_t *, unsigned int));
int pthread_tad_init		__P_((pthread_tad_t *, unsigned int));
int pthread_tad_destroy		__P_((pthread_tad_t *));

int pthread_atexit_add		__P_((pthread_atexit_t *, void (*)(void *), void *));
int pthread_atexit_remove	__P_((pthread_atexit_t *, int));


void	fclose_nrv			__P_((void *));
void	fflush_nrv			__P_((void *));
void	pthread_attr_destroy_nrv	__P_((void *));

__END_DECLS

