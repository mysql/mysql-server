/* ==== specific.c =======================================================
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
 * Description : Pthread thread specific data management.
 *
 *  1.20 94/03/30 proven
 *      -Started coding this file.
 */

#ifndef lint
static const char rcsid[] = "$Id$";
#endif

#include <errno.h>
#include <pthread.h>
#include <stdlib.h>
#include <string.h>

static struct pthread_key key_table[PTHREAD_DATAKEYS_MAX];
static pthread_mutex_t key_mutex = PTHREAD_MUTEX_INITIALIZER;

/* ==========================================================================
 * pthread_key_create()
 */
int pthread_key_create(pthread_key_t *key, void (*destructor)(void *))
{
	pthread_mutex_lock(&key_mutex);
	for ((*key) = 0; (*key) < PTHREAD_DATAKEYS_MAX; (*key)++) {
		if (key_table[(*key)].count == 0) {
			key_table[(*key)].count++;
			key_table[(*key)].destructor = destructor;
			pthread_mutex_init(&(key_table[(*key)].mutex), NULL);
			pthread_mutex_unlock(&key_mutex);
			return(OK);
		}
	}
	pthread_mutex_unlock(&key_mutex);
	return(EAGAIN);
}

/* ==========================================================================
 * pthread_key_delete()
 */
int pthread_key_delete(pthread_key_t key)
{
	int ret;

	if (key < PTHREAD_DATAKEYS_MAX) {
		pthread_mutex_lock(&(key_table[key].mutex));
		switch (key_table[key].count) {
		case 1:
			pthread_mutex_destroy(&(key_table[key].mutex));
			key_table[key].destructor = NULL;
			key_table[key].count = 0;
		case 0:
			ret = OK;
			break;
		default:
			ret = EBUSY;
		}
		pthread_mutex_unlock(&(key_table[key].mutex));
	} else {
		ret = EINVAL;
	}
	return(ret);
}

/* ==========================================================================
 * pthread_cleanupspecific()
 */
void pthread_cleanupspecific(void) 
{
	void * data;
	int key;
	int itr;

	pthread_mutex_lock(&key_mutex);
	for (itr = 0; itr < _POSIX_THREAD_DESTRUTOR_ITERATIONS; itr++) {
		for (key = 0; key < PTHREAD_DATAKEYS_MAX; key++) {
			if (pthread_run->specific_data_count) {
				if (pthread_run->specific_data[key]) {
					data = (void *)pthread_run->specific_data[key];
					pthread_run->specific_data[key] = NULL;
					pthread_run->specific_data_count--;
					if (key_table[key].destructor) {
						pthread_mutex_unlock(&key_mutex);
						key_table[key].destructor(data);
						pthread_mutex_lock(&key_mutex);
					}
					key_table[key].count--;
				}
			} else {
				free(pthread_run->specific_data);
				pthread_mutex_unlock(&key_mutex);
				return;
			}
		}
	}
	free(pthread_run->specific_data);
	pthread_mutex_unlock(&key_mutex);
}

static inline const void ** pthread_key_allocate_data(void)
{
	const void ** new_data;
	if(new_data = (const void**)malloc(sizeof(void *) * PTHREAD_DATAKEYS_MAX)) {
		memset((void *)new_data, 0, sizeof(void *) * PTHREAD_DATAKEYS_MAX);
	}
	return(new_data);
}

/* ==========================================================================
 * pthread_setspecific()
 */
int pthread_setspecific(pthread_key_t key, const void * value)
{
	int ret;

	if ((pthread_run->specific_data) ||
	  (pthread_run->specific_data = pthread_key_allocate_data())) {
		if ((key < PTHREAD_DATAKEYS_MAX) && (key_table)) {
			pthread_mutex_lock(&(key_table[key].mutex));
			if (key_table[key].count) {
				if (pthread_run->specific_data[key] == NULL) {
					if (value != NULL) {
						pthread_run->specific_data_count++;
						key_table[key].count++;
					}
				} else {
					if (value == NULL) {
						pthread_run->specific_data_count--;
						key_table[key].count--;
					}
				}
				pthread_run->specific_data[key] = value;
				ret = OK;
			} else {
				ret = EINVAL;
			}
			pthread_mutex_unlock(&(key_table[key].mutex));
		} else {
			ret = EINVAL;
		}
	} else {
		ret = ENOMEM;
	}
	return(ret);
}

/* ==========================================================================
 * pthread_getspecific()
 */
void * pthread_getspecific(pthread_key_t key)
{
	void *ret;

	if ((pthread_run->specific_data) && (key < PTHREAD_DATAKEYS_MAX)
      && (key_table)) {
		pthread_mutex_lock(&(key_table[key].mutex));
		if (key_table[key].count) {
			ret = (void *)pthread_run->specific_data[key];
		} else {
			ret = NULL;
		}
		pthread_mutex_unlock(&(key_table[key].mutex));
	} else {
		ret = NULL;
	}
	return(ret);
}
