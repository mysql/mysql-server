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
/*static char *sccsid = "from: @(#)getprotoent.c	5.8 (Berkeley) 2/24/91";*/
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
#include "proto_internal.h"

static pthread_mutex_t proto_file_lock = PTHREAD_MUTEX_INITIALIZER;
static int proto_file_stayopen;
static FILE *proto_file;

void setprotoent(int stayopen)
{
	pthread_mutex_lock(&proto_file_lock);
	proto_file_stayopen |= stayopen;
	if (proto_file)
		rewind(proto_file);
	else
		proto_file = fopen(_PATH_PROTOCOLS, "r");
	pthread_mutex_unlock(&proto_file_lock);
}

void endprotoent()
{
	pthread_mutex_lock(&proto_file_lock);
	if (proto_file)
		fclose(proto_file);
	pthread_mutex_unlock(&proto_file_lock);
}

struct protoent *getprotoent()
{
	char *buf = _proto_buf();

	return getprotoent_r((struct protoent *) buf,
						 buf + sizeof(struct protoent), PROTO_BUFSIZE);
}

struct protoent *getprotoent_r(struct protoent *result, char *buf, int bufsize)
{
	char *p, *q, **alias;
	int l;
	
	errno = 0;
	pthread_mutex_lock(&proto_file_lock);
	if (proto_file == NULL && !(proto_file = fopen(_PATH_PROTOCOLS, "r"))) {
		pthread_mutex_unlock(&proto_file_lock);
		return NULL;
	}
	while (fgets(buf, bufsize, proto_file)) {
		if (*buf == '#')
			continue;
		p = strpbrk(buf, "#\n");
		if (p == NULL)
			continue;
		*p = '\0';
		l = strlen(buf) + 1;
		result->p_name = buf;
		p = strpbrk(buf, " \t");
		if (p == NULL)
			continue;
		*p++ = '\0';
		while (*p == ' ' || *p == '\t')
			p++;
		q = strpbrk(p, " \t");
		if (q != NULL)
			*q++ = '\0';
		if (SP(SP(buf, char, l), char *, 1) > buf + bufsize) {
			errno = ERANGE;
			break;
		}
		result->p_proto = atoi(p);
		result->p_aliases = (char **) ALIGN(buf + l, char *);
		alias = result->p_aliases;
		if (q != NULL) {
			p = q;
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
		}
		*alias = NULL;
		pthread_mutex_unlock(&proto_file_lock);
		return result;
	}

	pthread_mutex_unlock(&proto_file_lock);
	return NULL;
}

