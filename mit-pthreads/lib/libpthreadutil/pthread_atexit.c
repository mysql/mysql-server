/* ==== pthread_atexit.c =====================================================
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
 * Description : Pthread attribute functions.
 *
 *  1.20 94/02/13 proven
 *      -Started coding this file.
 */

#ifndef lint
static const char rcsid[] = "$Id$";
#endif

#define PTHREAD_KERNEL

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include "pthreadutil.h"

static int pthread_atexit_inited = 0;
static pthread_key_t pthread_atexit_key;
static pthread_mutex_t pthread_atexit_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ==========================================================================
 * pthread_atexit_done()
 */
static void pthread_atexit_done(void * arg) 
{
	pthread_atexit_t id, id_next;

	for (id = arg; id; id = id_next) {
		id_next = id->next;
		id->rtn(id->arg);
		free(id);
	}
}

/* ==========================================================================
 * pthread_atexit_add()
 */
int pthread_atexit_add(pthread_atexit_t *id, void (*rtn)(void *), void * arg)
{
	int ret;

	if (ret = pthread_mutex_lock(&pthread_atexit_mutex)) {
		return(ret);
	}
	if (!pthread_atexit_inited) {
		if (ret = pthread_key_create(&pthread_atexit_key, pthread_atexit_done)){
			pthread_mutex_unlock(&pthread_atexit_mutex);
			return(ret);
		}
		pthread_atexit_inited++;
	}
	pthread_mutex_unlock(&pthread_atexit_mutex);
	
	if ((*id) = (pthread_atexit_t)malloc(sizeof(struct pthread_atexit))) {
		if ((*id)->next = pthread_getspecific(pthread_atexit_key)) {
			(*id)->next->prev = (*id);
		}
		pthread_setspecific(pthread_atexit_key, (void *)*id);
		(*id)->prev = NULL;
		(*id)->rtn = rtn;
		(*id)->arg = arg;
		return(OK);
	}
	return(ENOMEM);
}

/* ==========================================================================
 * pthread_atexit_remove()
 */
int pthread_atexit_remove(pthread_atexit_t * id, int execute)
{
	pthread_atexit_t old;

	if (old = pthread_getspecific(pthread_atexit_key)) {
		if (old == *id) {
			old = old->next;
			old->prev = NULL;
			pthread_setspecific(pthread_atexit_key, old);
		} else {
			if ((*id)->next) {
				(*id)->next->prev = (*id)->prev;
			}
			(*id)->prev->next = (*id)->next;
		}
		if (execute) {
			(*id)->rtn((*id)->arg);
		}
		free((*id));
		return(OK);
	}
	return(EINVAL);
}

/* ==========================================================================
 * A few non void functions that are often used as void functions
 */
void fflush_nrv(void * fp) { fflush((FILE *)fp); }
void fclose_nrv(void * fp) { fclose((FILE *)fp); }

void pthread_attr_destroy_nrv(void * attr) 
{ 
	pthread_attr_destroy((pthread_attr_t *)attr); 
}
