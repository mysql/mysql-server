/*
 * Copyright (c) 1983 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *	  notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *	  notice, this list of conditions and the following disclaimer in the
 *	  documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *	  must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *	  may be used to endorse or promote products derived from this software
 *	  without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.	IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

#if defined(LIBC_SCCS) && !defined(lint)
/*static char *sccsid = "from: @(#)inet_ntoa.c	5.6 (Berkeley) 2/24/91";*/
static char *rcsid = "$Id$";
#endif /* LIBC_SCCS and not lint */

/*
 * Convert network-format internet address
 * to base 256 d.d.d.d representation.
 */
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <arpa/inet.h>

char *inet_ntoa(struct in_addr in)
{
	static pthread_mutex_t inet_ntoa_mutex = PTHREAD_MUTEX_INITIALIZER;
	static pthread_key_t inet_ntoa_key = -1;
	char *buf, *inet_ntoa_r();

	if (inet_ntoa_key < 0) {
		pthread_mutex_lock(&inet_ntoa_mutex);
		if (inet_ntoa_key < 0) {
			if (pthread_key_create(&inet_ntoa_key, free) < 0) {
				pthread_mutex_unlock(&inet_ntoa_mutex);
				return(NULL);
			}
		}
		pthread_mutex_unlock(&inet_ntoa_mutex);
	}
	if ((buf = pthread_getspecific(inet_ntoa_key)) == NULL) {
		if ((buf = (char *) malloc(18)) == NULL) {
			return(NULL);
		}
		pthread_setspecific(inet_ntoa_key, buf);
	}
	return inet_ntoa_r(in, buf, 18);
}

char *inet_ntoa_r(struct in_addr in, char *buf, int bufsize)
{
	register char *p;

	p = (char *)&in;
#define	UC(b)	(((int)b)&0xff)
	(void)snprintf(buf, bufsize,
				   "%d.%d.%d.%d", UC(p[0]), UC(p[1]), UC(p[2]), UC(p[3]));
	return (buf);
}

