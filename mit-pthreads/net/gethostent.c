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
/*static char *sccsid = "from: @(#)gethostent.c	5.8 (Berkeley) 2/24/91";*/
static char *rcsid = "$Id$";
#endif /* LIBC_SCCS and not lint */

#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netdb.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "res_internal.h"

static pthread_mutex_t res_file_lock = PTHREAD_MUTEX_INITIALIZER;
static int res_file_stayopen;
static FILE *res_file;

void sethostent(int stayopen)
{
	pthread_mutex_lock(&res_file_lock);
	res_file_stayopen |= stayopen;
	if (res_file)
		rewind(res_file);
	else
		res_file = fopen(_PATH_HOSTS, "r");
	pthread_mutex_unlock(&res_file_lock);
}

void endhostent()
{
	pthread_mutex_lock(&res_file_lock);
	if (res_file)
		fclose(res_file);
	pthread_mutex_unlock(&res_file_lock);
}

struct hostent *gethostent()
{
	struct res_data *data = _res_init();
	
	if (!data)
		return NULL;
	if (!data->buf) {
		data->buf = malloc(sizeof(struct hostent) + HOST_BUFSIZE);
		if (!data->buf) {
			data->errval = NO_RECOVERY;
			return NULL;
		}
	}
	return gethostent_r((struct hostent *) data->buf,
						data->buf + sizeof(struct hostent), HOST_BUFSIZE,
						&data->errval);
}

struct hostent *gethostent_r(struct hostent *result, char *buf, int bufsize,
							 int *errval)
{
	char *p, **alias;
	struct in_addr *addr;
	int l;

	errno = 0;
	pthread_mutex_lock(&res_file_lock);
	if (res_file == NULL && (res_file = fopen(_PATH_HOSTS, "r")) == NULL) {
		pthread_mutex_unlock(&res_file_lock);
		return NULL;
	}
	while (fgets(buf, bufsize, res_file)) {
		if (*buf == '#')
			continue;
		p = strpbrk(buf, "#\n");
		if (p == NULL)
			continue;
		l = strlen(buf) + 1;
		*p = '\0';
		p = strpbrk(buf, " \t");
		if (p == NULL)
			continue;
		*p++ = '\0';

		/* THIS STUFF IS INTERNET SPECIFIC */
		if (SP(SP(SP(buf, char, l), *addr, 1), char *, 3) > buf + bufsize) {
			errno = ERANGE;
			break;
		}
		addr = (struct in_addr *) ALIGN(buf + l, struct in_addr);
		if (inet_aton(buf, addr) == 0)
			continue;
		result->h_length = sizeof(*addr);
		result->h_addrtype = AF_INET;
		result->h_addr_list = (char **) ALIGN(addr + sizeof(*addr), char *);
		result->h_addr_list[0] = (char *) addr;
		result->h_addr_list[1] = NULL;
		result->h_aliases = result->h_addr_list + 2;
		while (*p == ' ' || *p == '\t')
			p++;
		result->h_name = p;
		alias = result->h_aliases;
		p = strpbrk(p, " \t");
		if (p != NULL) 
			*p++ = '\0';
		while (p && *p) {
			if (*p == ' ' || *p == '\t') {
				p++;
				continue;
			}
			if ((char *) &alias[2] > buf + bufsize) {
				errno = ERANGE;
				break;
			}
			*alias++ = p;
			p = strpbrk(p, " \t");
			if (p != NULL)
				*p++ = '\0';
		}
		if (p && *p)
			break;
		*alias = NULL;
		pthread_mutex_unlock(&res_file_lock);
		return result;
	}

	pthread_mutex_unlock(&res_file_lock);
	*errval = (errno == ERANGE) ? NO_RECOVERY : 0;
	return NULL;
}

