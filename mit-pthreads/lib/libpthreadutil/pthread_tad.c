/* ==== pthread_tad.c =========================================================
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

#ifndef lint
static char copyright[] =
 "@(#) Copyright (c) 1995 Chris Provenzano.\nAll rights reserved.\n";
#endif /* not lint */

/* tad = thread allocation domain */
#define PTHREAD_KERNEL

#include "pthreadutil.h"
#include <stdio.h>
#include <errno.h>

int pthread_tad_count(pthread_tad_t * tad)
{
	int ret;

	pthread_mutex_lock(&tad->mutex);
	ret = tad->count_current;
	pthread_mutex_unlock(&tad->mutex);
	return(ret);
}

static void pthread_tad_done(void * arg)
{
	pthread_tad_t * tad = arg;
	pthread_mutex_lock(&tad->mutex);
	--tad->count_current;
/*	if (--tad->count_current < tad->count_max) */
	pthread_cond_broadcast(&tad->cond);
	pthread_mutex_unlock(&tad->mutex);
}

#ifndef PTHREAD_KERNEL
struct tad_start {
	pthread_tad_t *	tad;
	void *			(*routine)();
	void *			arg;
};

static void * pthread_tad_start(struct tad_start * tad_start)
{
	void * (*routine)() = tad_start->routine;
	void * arg = tad_start->arg;

	pthread_mutex_lock(&tad_start->tad->mutex);
	pthread_cleanup_push(pthread_tad_done, tad_start->tad);
	pthread_mutex_unlock(&tad_start->tad->mutex);
	free(tad_start);
	return(routine(arg));
}
#else
static void * pthread_tad_start(void * tad_start_arg)
{
	pthread_tad_t * tad = tad_start_arg;
	void * (*routine)() = tad->routine;
	void * arg = tad->arg;

	tad->count_current++;
	pthread_cleanup_push(pthread_tad_done, tad);
	pthread_mutex_unlock(&tad->mutex);
	return(routine(arg));
}
#endif

int pthread_tad_create(pthread_tad_t * tad, pthread_t *thread_id, 
  pthread_attr_t *attr, void * (*routine)(), void * arg)
{
#ifndef PTHREAD_KERNEL
	struct tad_start tad;
#endif
	int ret;

	pthread_mutex_lock(&tad->mutex);
	while (tad->count_max && (tad->count_current > tad->count_max)) 
		pthread_cond_wait(&tad->cond, &tad->mutex);

#ifndef PTHREAD_KERNEL
	if ((tad_start = malloc(sizeof(struct tad_start))) == NULL) { 
		pthread_mutex_unlock(&tad->mutex);
		return(ENOMEM);
	}
	tad_start->routine = routine;
	tad_start->arg = arg;
	tad_start->tad = tad;
	if ((ret = pthread_create(thread_id, attr, 
	  pthread_tad_start, tad_start)) == OK) 
		tad->count_current++;
	pthread_mutex_unlock(&tad->mutex);
#else
	tad->routine = routine;
	tad->arg = arg;
	if (ret = pthread_create(thread_id, attr, pthread_tad_start, tad))
		pthread_mutex_unlock(&tad->mutex);
#endif
	return(ret);
}
	
int pthread_tad_wait(pthread_tad_t * tad, unsigned int count)
{
	if ((tad->count_max) && (tad->count_max < count)) {
		return(EINVAL);
	}
	pthread_mutex_lock(&tad->mutex);
	while (tad->count_current > count) 
		pthread_cond_wait(&tad->cond, &tad->mutex);
	pthread_mutex_unlock(&tad->mutex);
	return(OK);
}

int pthread_tad_init(pthread_tad_t * tad, unsigned int max_count) 
{
	int ret;

	if ((ret = pthread_mutex_init(&tad->mutex, NULL)) == OK) {
		if (ret = pthread_cond_init(&tad->cond, NULL)) {
			pthread_mutex_destroy(&tad->mutex);
		} else {
			tad->count_max = max_count;
			tad->count_current = 0;
		} 
	}
	return(ret);
}

/* User is responsible to make sure their are no threads running */
int pthread_tad_destroy(pthread_tad_t * tad) 
{
	int ret;

	if ((ret = pthread_mutex_destroy(&tad->mutex)) == OK) {
		ret = pthread_cond_destroy(&tad->cond);
	} else {
		pthread_cond_destroy(&tad->cond);
	}
	tad->count_max = NOTOK;
	return(ret);
}
