/* ==== pthread_attr.c =======================================================
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
 * Description : Pthread attribute functions.
 *
 *  1.00 93/11/04 proven
 *      -Started coding this file.
 */

#ifndef lint
static const char rcsid[] = "$Id$";
#endif

#include <pthread.h>
#include <errno.h>
#include <string.h>

/* Currently we do no locking, should we just to be safe? CAP */
/* ==========================================================================
 * pthread_attr_init()
 */
int pthread_attr_init(pthread_attr_t *attr)
{
	memcpy(attr, &pthread_attr_default, sizeof(pthread_attr_t));
	return(OK);
}

/* ==========================================================================
 * pthread_attr_destroy()
 */
int pthread_attr_destroy(pthread_attr_t *attr)
{
	return(OK);
}

/* ==========================================================================
 * pthread_attr_getstacksize()
 */
int pthread_attr_getstacksize(pthread_attr_t *attr, size_t * stacksize)
{
	*stacksize = attr->stacksize_attr;
	return(OK);
}

/* ==========================================================================
 * pthread_attr_setstacksize()
 */
int pthread_attr_setstacksize(pthread_attr_t *attr, size_t stacksize)
{
	if (stacksize >= PTHREAD_STACK_MIN) {
		attr->stacksize_attr = stacksize;
		return(OK);
	}
	return(EINVAL);
}

/* ==========================================================================
 * pthread_attr_getstackaddr()
 */
int pthread_attr_getstackaddr(pthread_attr_t *attr, void ** stackaddr)
{
	*stackaddr = attr->stackaddr_attr;
	return(OK);
}

/* ==========================================================================
 * pthread_attr_setstackaddr()
 */
int pthread_attr_setstackaddr(pthread_attr_t *attr, void * stackaddr)
{
	attr->stackaddr_attr = stackaddr;
	return(OK);
}

/* ==========================================================================
 * pthread_attr_setcleanup()
 */
int pthread_attr_setcleanup(pthread_attr_t *attr, void (*routine)(void *),
  void * arg)
{
	attr->cleanup_attr = routine;
	attr->arg_attr = arg;
	return(OK);
}

/* ==========================================================================
 * pthread_attr_getdetachstate()
 */
int pthread_attr_getdetachstate(pthread_attr_t *attr, int * detachstate)
{
	*detachstate = attr->flags & PTHREAD_DETACHED;
	return(OK);
}

/* ==========================================================================
 * pthread_attr_setdetachstate()
 */
int pthread_attr_setdetachstate(pthread_attr_t *attr, int detachstate)
{
	attr->flags = (attr->flags & ~(PTHREAD_DETACHED)) |
	  (detachstate & PTHREAD_DETACHED);
	return(OK);
}

/* ==========================================================================
 * pthread_attr_getfloatstate()
 */
int pthread_attr_getfloatstate(pthread_attr_t *attr, int * floatstate)
{
	*floatstate = attr->flags & PTHREAD_NOFLOAT;
	return(OK);
}

/* ==========================================================================
 * pthread_attr_setfloatstate()
 */
int pthread_attr_setfloatstate(pthread_attr_t *attr, int floatstate)
{
	attr->flags = (attr->flags & ~(PTHREAD_NOFLOAT)) |
	  (floatstate & PTHREAD_NOFLOAT);
	return(OK);
}

/* ==========================================================================
 * pthread_attr_getscope()
 */
int pthread_attr_getscope(pthread_attr_t *attr, int * contentionscope)
{
	*contentionscope = attr->flags & PTHREAD_SCOPE_SYSTEM;
	return(OK);
}

/* ==========================================================================
 * pthread_attr_setscope()
 */
int pthread_attr_setscope(pthread_attr_t *attr, int contentionscope)
{
	int ret;

	switch (contentionscope) {
	case PTHREAD_SCOPE_PROCESS:
		attr->flags = (attr->flags & ~(PTHREAD_SCOPE_PROCESS)) 
		  | PTHREAD_SCOPE_PROCESS;
		ret = OK;
		break;
	case PTHREAD_SCOPE_SYSTEM:
		ret = ENOSYS;
		break;
	default:
		ret = EINVAL;
		break;
	}

	return(ret);
}

/* ==========================================================================
 * pthread_attr_getinheritsched()
 */
int pthread_attr_getinheritsched(pthread_attr_t *attr, int * inheritsched)
{
	*inheritsched = attr->flags & PTHREAD_INHERIT_SCHED;
	return(OK);
}

/* ==========================================================================
 * pthread_attr_setinheritsched()
 */
int pthread_attr_setinheritsched(pthread_attr_t *attr, int inheritsched)
{
	attr->flags = (attr->flags & ~(PTHREAD_INHERIT_SCHED)) |
	  (inheritsched & PTHREAD_INHERIT_SCHED);
	return(OK);
}

/* ==========================================================================
 * pthread_attr_getschedpolicy()
 */
int pthread_attr_getschedpolicy(pthread_attr_t *attr, int * schedpolicy)
{
	*schedpolicy = (int)attr->schedparam_policy;
	return(OK);
}

/* ==========================================================================
 * pthread_attr_setschedpolicy()
 */
int pthread_attr_setschedpolicy(pthread_attr_t *attr, int schedpolicy)
{
	int ret;

	switch(schedpolicy) {
	case SCHED_FIFO:
	case SCHED_IO:
	case SCHED_RR:
		attr->schedparam_policy = schedpolicy;
		ret = OK;
		break;
	default:
		ret = EINVAL;
		break;
	}
	return(ret);
}

/* ==========================================================================
 * pthread_attr_getschedparam()
 */
int pthread_attr_getschedparam(pthread_attr_t *attr, struct sched_param * param)
{
	param->sched_priority = attr->sched_priority;
	return(OK);
}

/* ==========================================================================
 * pthread_attr_setschedparam()
 */
int pthread_attr_setschedparam(pthread_attr_t *attr, struct sched_param * param)
{
	if ((param->sched_priority >= PTHREAD_MIN_PRIORITY) &&
	  (param->sched_priority <= PTHREAD_MAX_PRIORITY)) {
		attr->sched_priority = param->sched_priority;
		return(OK);
	}
	return(EINVAL);
}

