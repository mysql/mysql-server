/*
 * Copyright (c) 1985, 1988 Regents of the University of California.
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
/*static char *sccsid = "from: @(#)gethostbyname.c	6.45 (Berkeley) 2/24/91";*/
static char *rcsid = "$Id$";
#endif /* LIBC_SCCS and not lint */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <resolv.h>
#include "res_internal.h"

static struct hostent *fake_hostent(const char *hostname, struct in_addr addr,
									struct hostent *result, char *buf,
									int bufsize, int *errval);
static struct hostent *file_find_name(const char *name, struct hostent *result,
									  char *buf, int bufsize, int *errval);

struct hostent *gethostbyname(const char *hostname)
{
	struct res_data *data = _res_init();

	if (!data)
		return NULL;
	if (!data->buf) {
		data->buf = malloc(sizeof(struct hostent) + HOST_BUFSIZE);
		if (!data->buf) {
			errno = 0;
			data->errval = NO_RECOVERY;
			return NULL;
		}
	}
	return gethostbyname_r(hostname, (struct hostent *) data->buf,
						   data->buf + sizeof(struct hostent), HOST_BUFSIZE,
						   &data->errval);
}

struct hostent *gethostbyname_r(const char *hostname, struct hostent *result,
								char *buf, int bufsize, int *errval)
{
	struct in_addr addr;
	querybuf qbuf;
	const char *p;
	int n;

	/* Default failure condition is not a range error and not recoverable. */
	errno = 0;
	*errval = NO_RECOVERY;
	
	/* Check for all-numeric hostname with no trailing dot. */
	if (isdigit(hostname[0])) {
		p = hostname;
		while (*p && (isdigit(*p) || *p == '.'))
			p++;
		if (!*p && p[-1] != '.') {
			/* Looks like an IP address; convert it. */
			if (inet_aton(hostname, &addr) == -1) {
				*errval = HOST_NOT_FOUND;
				return NULL;
			}
			return fake_hostent(hostname, addr, result, buf, bufsize, errval);
		}
	}
	
	/* Do the search. */
	n = res_search(hostname, C_IN, T_A, qbuf.buf, sizeof(qbuf));
	if (n >= 0)
		return _res_parse_answer(&qbuf, n, 0, result, buf, bufsize, errval);
	else if (errno == ECONNREFUSED)
		return file_find_name(hostname, result, buf, bufsize, errval);
	else
		return NULL;
}

static struct hostent *fake_hostent(const char *hostname, struct in_addr addr,
									struct hostent *result, char *buf,
									int bufsize, int *errval)
{
	int len = strlen(hostname);
	char *name, *addr_ptr;

	if (SP(SP(SP(buf, char, len + 1), addr, 1), char *, 3) > buf + bufsize) {
		errno = ERANGE;
		return NULL;
	}

	/* Copy faked name and address into buffer. */
	strcpy(buf, hostname);
	name = buf;
	buf = ALIGN(buf + len + 1, addr);
	*((struct in_addr *) buf) = addr;
	addr_ptr = buf;
	buf = ALIGN(buf + sizeof(addr), char *);
	((char **) buf)[0] = addr_ptr;
	((char **) buf)[1] = NULL;
	((char **) buf)[2] = NULL;

	result->h_name = name;
	result->h_aliases = ((char **) buf) + 2;
	result->h_addrtype = AF_INET;
	result->h_length = sizeof(addr);
	result->h_addr_list = (char **) buf;

	return result;
}

static struct hostent *file_find_name(const char *name, struct hostent *result,
				      char *buf, int bufsize, int *errval)
{
	char **alias;
	FILE *fp = NULL;

	pthread_mutex_lock(&host_iterate_lock);
	sethostent(0);
	while ((result = gethostent_r(result, buf, bufsize, errval)) != NULL) {
		/* Check the entry's name and aliases against the given name. */
		if (strcasecmp(result->h_name, name) == 0)
			break;
		for (alias = result->h_aliases; *alias; alias++) {
			if (strcasecmp(*alias, name) == 0)
			  goto end;		/* Josip Gracin */
		}
	}
end:
	pthread_mutex_unlock(&host_iterate_lock);
	if (!result && errno != ERANGE)
		*errval = HOST_NOT_FOUND;
	return result;
}

