/*
 * Copyright (c) 1985 Regents of the University of California.
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
/*static char *sccsid = "from: @(#)res_query.c	6.22 (Berkeley) 3/19/91";*/
static char *rcsid = "$Id$";
#endif /* LIBC_SCCS and not lint */

#include <pthread.h>
#include <resolv.h>
#include <netdb.h>
#include "res_internal.h"

#if PACKETSZ > 1024
#define MAXPACKET	PACKETSZ
#else
#define MAXPACKET	1024
#endif

int res_query(char *name, int class, int type, unsigned char *answer,
			  int anslen)
{
	struct res_data *data;
	char buf[MAXPACKET];
	int result;
	HEADER *hp;

	data = _res_init();
	if (!data)
		return -1;

	/* Make the query. */
	result = res_mkquery(QUERY, name, class, type, NULL, 0, NULL, buf,
						 sizeof(buf));
	if (result <= 0) {
		data->errval = NO_RECOVERY;
		return result;
	}

	result = res_send(buf, result, (char *) answer, anslen);
	if (result < 0) {
		data->errval = TRY_AGAIN;
		return result;
	}

	hp = (HEADER *) answer;
	if (hp->rcode == NOERROR && ntohs(hp->ancount) != 0)
	    return result;

	/* Translate the error code and return. */
	switch(hp->rcode) {
	  case NOERROR:
		data->errval = NO_DATA;
		break;
	  case SERVFAIL:
		data->errval = TRY_AGAIN;
		break;
	  case NXDOMAIN:
		data->errval = HOST_NOT_FOUND;
		break;
	  default:
		data->errval = NO_RECOVERY;
		break;
	}
	return -1;
}

