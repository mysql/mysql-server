/* ==== condattr.c ===========================================================
 * Copyright (c) 1995 by Chris Provenzano, proven@mit.edu
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
 * Description : Mutex functions.
 *
 *  1.00 95/08/22 proven
 *      -Started coding this file.
 */

#ifndef lint
static const char rcsid[] = "$Id$";
#endif

#include <pthread.h>
#include <errno.h>

/* ==========================================================================
 * pthread_condattr_init()
 */
int pthread_condattr_init(pthread_condattr_t *attr)
{
	attr->c_type = COND_TYPE_FAST;
	return(OK);
}

/* ==========================================================================
 * pthread_condattr_destroy()
 */
int pthread_condattr_destroy(pthread_condattr_t *attr)
{
	return(OK);
}

/* ==========================================================================
 * pthread_condattr_settype()
 */
int pthread_condattr_settype(pthread_condattr_t *attr, unsigned int type)
{
	switch(type) {
	case PTHREAD_CONDTYPE_FAST:
		attr->c_type = COND_TYPE_FAST;
		break;
	case PTHREAD_CONDTYPE_RECURSIVE:
		attr->c_type = COND_TYPE_COUNTING_FAST;
		break;
	case PTHREAD_CONDTYPE_DEBUG:
		attr->c_type = COND_TYPE_DEBUG;
		break;
	default:
		return(EINVAL);
	}
	return(OK);
}

/* ==========================================================================
 * pthread_condattr_gettype()
 */
int pthread_condattr_gettype(pthread_condattr_t *attr, unsigned int * type)
{
	*type = (unsigned int)attr->c_type;
	return(OK);
}
