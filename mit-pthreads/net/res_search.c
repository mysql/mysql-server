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
/*static char *sccsid = "from: @(#)res_search.c	6.45 (Berkeley) 2/24/91";*/
static char *rcsid = "$Id$";
#endif /* LIBC_SCCS and not lint */

#include <pthread.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <resolv.h>
#include <netdb.h>
#include "res_internal.h"

static char *search_aliases(const char *name, char *buf, int bufsize);

int res_search(const char *name, int class, int type, unsigned char *answer,
			   int anslen)
{
	struct res_data *data;
	const char *p;
	int num_dots, len, result, no_data = 0, error;
	char buf[2 * MAXDNAME + 2], *domain, **dptr, *alias;

	data = _res_init();
	if (!data)
		return -1;

	/* Count the dots in name, and get a pointer to the end of name. */
	num_dots = 0;
	for (p = name; *p; p++) {
		if (*p == '.')
			num_dots++;
	}
	len = p - name;

	/* If there aren't any dots, check to see if name is an alias for
	 * another host.  If so, try the resolved alias as a fully-qualified
	 * name. */
	alias = search_aliases(name, buf, sizeof(buf));
	if (alias != NULL)
		return res_query(alias, class, type, answer, anslen);

	/* If there's a trailing dot, try to strip it off and query the name. */
	if (len > 0 && p[-1] == '.') {
		if (len > sizeof(buf)) {
			/* It's too long; just query the original name. */
			return res_query(name, class, type, answer, anslen);
		} else {
			/* Copy the name without the trailing dot and query. */
			memcpy(buf, name, len - 1);
			buf[len] = 0;
			return res_query(buf, class, type, answer, anslen);
		}
	}

	if (data->state.options & RES_DNSRCH) {
		/* If RES_DNSRCH is set, query all the domains until we get a
		 * definitive answer. */
		for (dptr = data->state.dnsrch; *dptr; dptr++) {
			domain = *dptr;
			sprintf(buf, "%.*s.%.*s", MAXDNAME, name, MAXDNAME, domain);
			result = res_query(buf, class, type, answer, anslen);
			if (result > 0)
				return result;
			if (data->errval == NO_DATA)
				no_data = 1;
			else if (data->errval != HOST_NOT_FOUND)
				break;
		}
	} else if (num_dots == 0 && data->state.options & RES_DEFNAMES) {
		/* If RES_DEFNAMES is set and there is no dot, query the default
		 * domain. */
		domain = data->state.defdname;
		sprintf(buf, "%.*s.%.%s", MAXDNAME, name, MAXDNAME, domain);
		result = res_query(buf, class, type, answer, anslen);
		if (result > 0)
			return result;
		if (data->errval == NO_DATA)
			no_data = 1;
	}

	/* If all the domain queries failed, try the name as fully-qualified.
	 * Only do this if there is at least one dot in the name. */
	if (num_dots > 0) {
		result = res_query(name, class, type, answer, anslen);
		if (result > 0)
			return result;
	}

	if (no_data)
		data->errval = NO_DATA;

	return -1;
}

static char *search_aliases(const char *name, char *buf, int bufsize)
{
	FILE *fp;
	char *filename, *p;
	int len;

	filename = getenv("HOSTALIASES");
	if (filename == NULL)
		return NULL;

	fp = fopen(filename, "r");
	if (fp == NULL)
		return NULL;

	len = strlen(name);
	while (fgets(buf, bufsize, fp)) {

		/* Get the first word from the buffer. */
		p = buf;
		while (*p && !isspace(*p))
			p++;
		if (!*p)
			break;

		/* Null-terminate the first word and compare it with the name. */
		*p = 0;
		if (strcasecmp(buf, name) != 0)
			continue;

		p++;
		while (isspace(*p))
			p++;
		fclose(fp);
		return (*p) ? p : NULL;
	}

	fclose(fp);
	return NULL;
}

