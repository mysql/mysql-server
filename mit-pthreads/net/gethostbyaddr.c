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
/*static char *sccsid = "from: @(#)gethostbyaddr.c	6.45 (Berkeley) 2/24/91";*/
static char *rcsid = "$Id$";
#endif /* LIBC_SCCS and not lint */


#include <pthread.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <netdb.h>
#include <netinet/in.h>
#include <resolv.h>
#include <netinet/in.h>
#include <errno.h>
#include <sys/socket.h>
#include "res_internal.h"

static struct hostent *file_find_addr(const char *addr, int len, int type,
									  struct hostent *result, char *buf,
									  int bufsize, int *errval);

struct hostent *gethostbyaddr(const char *addr, int len, int type)
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
	return gethostbyaddr_r(addr, len, type, (struct hostent *) data->buf,
						   data->buf + sizeof(struct hostent), HOST_BUFSIZE,
						   &data->errval);
}

struct hostent *gethostbyaddr_r(const char *addr, int len, int type,
								struct hostent *result, char *buf, int bufsize,
								int *errval)
{
	struct res_data *data;
	querybuf qbuf;
	char lookups[MAXDNSLUS], addrbuf[MAXDNAME], *abuf;
	struct hostent *hp;
	int n, i;

	/* Default failure condition is not a range error and not recoverable. */
	errno = 0;
	*errval = NO_RECOVERY;
	
	data = _res_init();
	if (!data)
		return NULL;

	if (type != AF_INET)
		return NULL;
	sprintf(addrbuf, "%u.%u.%u.%u.in-addr.arpa",
			(unsigned)addr[3] & 0xff, (unsigned)addr[2] & 0xff,
			(unsigned)addr[1] & 0xff, (unsigned)addr[0] & 0xff);

	memcpy(lookups, data->state.lookups, sizeof(lookups));
	if (*lookups == 0)
		strncpy(lookups, "bf", sizeof(lookups));

	hp = NULL;
	for (i = 0; i < MAXDNSLUS && hp == NULL && lookups[i]; i++) {
		switch (lookups[i]) {
		  case	'b':

			/* Allocate space for a one-item list of addresses. */
			abuf = SP(SP(buf, char *, 2), struct in_addr, 1);
			if (abuf > buf + bufsize) {
				errno = ERANGE;
				return NULL;
			}

			/* Perform and parse the query. */
			n = res_query(addrbuf, C_IN, T_PTR, (char *)&qbuf, sizeof(qbuf));
			if (n < 0)
				break;
			hp = _res_parse_answer(&qbuf, n, 1, result, abuf,
								   bufsize - (abuf - buf), errval);
			if (hp == NULL)
				break;

			/* Fill in our own address list. */
			result->h_addrtype = type;
			result->h_length = len;
			result->h_addr_list = (char **) ALIGN(buf, char *);
			result->h_addr_list[0] = ALIGN(&result->h_addr_list[2],
										   struct in_addr);
			result->h_addr_list[1] = NULL;
			break;

		  case 'f':
			hp = file_find_addr(addr, len, type, result, buf, bufsize, errval);
			break;
		}
	}

	return hp;
}

static struct hostent *file_find_addr(const char *addr, int len, int type,
									  struct hostent *result, char *buf,
									  int bufsize, int *errval)
{
	FILE *fp = NULL;

	pthread_mutex_lock(&host_iterate_lock);
	sethostent(0);
	while ((result = gethostent_r(result, buf, bufsize, errval)) != NULL) {
		/* Check the entry against the given address. */
		if (result->h_addrtype == type &&
			memcmp(result->h_addr, addr, len) == 0)
			break;
	}
	pthread_mutex_unlock(&host_iterate_lock);
	if (!result && errno != ERANGE)
		*errval = HOST_NOT_FOUND;
	return result;
}

