/*
 * Copyright (c) 1995 John Birrell <jb@cimlogic.com.au>.
 * All rights reserved.
 *
 * Modified and extended by Antony T Curtis <antony.curtis@olcs.net>
 * for use with OS/2.
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
 *	This product includes software developed by John Birrell.
 * 4. Neither the name of the author nor the names of any co-contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY JOHN BIRRELL AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 */
#include <stdlib.h>
#include <errno.h>
#ifdef _THREAD_SAFE
//#include <pthread.h>
//#include "pthread_private.h"

int
pthread_mutex_init(pthread_mutex_t * mutex,
		   const pthread_mutexattr_t * mutex_attr)
{
	APIRET		rc = 0;

	rc = DosCreateMutexSem(NULL,mutex,0,0);

	/* Return the completion status: */
	return (0);
}

int
pthread_mutex_destroy(pthread_mutex_t * mutex)
{
	APIRET		rc = 0;


	do {
		rc = DosCloseMutexSem(*mutex);
		if (rc == 301) DosReleaseMutexSem(*mutex);
	} while (rc == 301);

	*mutex = 0;

	/* Return the completion status: */
	return (0);
}


int
pthread_mutex_lock(pthread_mutex_t * mutex)
{
	int             ret = 0;
	int             status = 0;
	APIRET		rc = 0;

   rc = DosRequestMutexSem(*mutex,SEM_INDEFINITE_WAIT);
	if (rc)
      return(EINVAL);
	/* Return the completion status: */
	return (0);
}

int
pthread_mutex_unlock(pthread_mutex_t * mutex)
{
	int             ret = 0;
	APIRET		rc = 0;
	int             status;

   rc = DosReleaseMutexSem(*mutex);

	/* Return the completion status: */
	return (0);
}
#endif
