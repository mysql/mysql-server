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
/*static char *sccsid = "from: @(#)res_internal.c	6.22 (Berkeley) 3/19/91";*/
static char *rcsid = "$Id$";
#endif /* LIBC_SCCS and not lint */

#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <resolv.h>
#include <netdb.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <string.h>
#include <errno.h>
#include "res_internal.h"

#define DEFAULT_RETRIES 4

pthread_mutex_t host_iterate_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_once_t init_once = PTHREAD_ONCE_INIT;
static pthread_key_t key;
static int init_status;

static void _res_init_global(void);
static void set_options(const char *options, const char *source);
static pthread_ipaddr_type net_mask(struct in_addr in);
static int qcomp(const void *arg1, const void *arg2);

static struct __res_state start;
/* We want to define _res for partial binary compatibility with libraries. */
#undef _res
struct __res_state _res = {
	RES_TIMEOUT,               	/* retransmition time interval */
	4,                         	/* number of times to retransmit */
	RES_DEFAULT,				/* options flags */
	1,                         	/* number of name servers */
};

struct hostent *_res_parse_answer(querybuf *answer, int anslen, int iquery,
								  struct hostent *result, char *buf,
								  int bufsize, int *errval)
{
	struct res_data *data = _res_init();
	register HEADER *hp;
	register u_char *cp;
	register int n;
	u_char *eom;
	char *aliases[__NETDB_MAXALIASES], *addrs[__NETDB_MAXADDRS];
	char *bp = buf, **ap = aliases, **hap = addrs;
	int type, class, ancount, qdcount, getclass = C_ANY, iquery_done = 0;

	eom = answer->buf + anslen;
	/*
	 * find first satisfactory answer
	 */
	hp = &answer->hdr;
	ancount = ntohs(hp->ancount);
	qdcount = ntohs(hp->qdcount);
	bp = buf;
	cp = answer->buf + sizeof(HEADER);

	/* Read in the hostname if this is an address lookup. */
	if (qdcount) {
		if (iquery) {
			if ((n = dn_expand((u_char *) answer->buf,
							   (u_char *) eom, (u_char *) cp, (u_char *) bp,
							   bufsize - (bp - buf))) < 0) {
				*errval = NO_RECOVERY;
				return ((struct hostent *) NULL);
			}
			cp += n + QFIXEDSZ;
			result->h_name = bp;
			bp += strlen(bp) + 1;
		} else {
			cp += __dn_skipname(cp, eom) + QFIXEDSZ;
		}
		while (--qdcount > 0)
			cp += __dn_skipname(cp, eom) + QFIXEDSZ;
	} else if (iquery) {
		*errval = (hp->aa) ? HOST_NOT_FOUND : TRY_AGAIN;
		return ((struct hostent *) NULL);
	}

	/* Read in the answers. */
	*ap = NULL;
	*hap = NULL;
	while (--ancount >= 0 && cp < eom) {
		if ((n = dn_expand((u_char *) answer->buf, (u_char *) eom,
						   (u_char *) cp, (u_char *) bp,
						   bufsize - (bp - buf))) < 0)
			break;
		cp += n;
		type = _getshort(cp);
		cp += sizeof(u_short);
		class = _getshort(cp);
		cp += sizeof(u_short) + sizeof(pthread_ipaddr_type);
		n = _getshort(cp);
		cp += sizeof(u_short);
		if (type == T_CNAME) {
			cp += n;
			if (ap >= aliases + __NETDB_MAXALIASES - 1)
				continue;
			*ap++ = bp;
			bp += strlen(bp) + 1;
			continue;
		}
		if (iquery && type == T_PTR) {
			if ((n = dn_expand((u_char *) answer->buf, (u_char *) eom,
							   (u_char *) cp, (u_char *) bp,
							   bufsize - (bp - buf))) < 0)
				break;
			cp += n;
			result->h_name = bp;
			bp += strlen(bp) + 1;
			iquery_done = 1;
			break;
		}
		if (iquery || type != T_A)	{
#ifdef DEBUG_RESOLVER
			if (data->state.options & RES_DEBUG)
				printf("unexpected answer type %d, size %d\n",
					   type, n);
#endif
			cp += n;
			continue;
		}
		if (hap > addrs) {
			if (n != result->h_length) {
				cp += n;
				continue;
			}
			if (class != getclass) {
				cp += n;
				continue;
			}
		} else {
			result->h_length = n;
			getclass = class;
			result->h_addrtype = (class == C_IN) ? AF_INET : AF_UNSPEC;
			if (!iquery) {
				result->h_name = bp;
				bp += strlen(bp) + 1;
			}
		}
		bp = ALIGN(bp, pthread_ipaddr_type);
		if (bp + n >= buf + bufsize) {
			errno = ERANGE;
			return NULL;
		}
		memcpy(bp, cp, n);
		cp += n;
		if (hap >= addrs + __NETDB_MAXADDRS - 1)
			continue;
		*hap++ = bp;
		bp += n;
		cp += n;
	}

	if (hap > addrs || iquery_done) {
		*ap++ = NULL;
		*hap++ = NULL;
		if (data->state.nsort)
			qsort(addrs, hap - addrs, sizeof(struct in_addr), qcomp);
		if (SP(bp, char *, (hap - addrs) + (ap - aliases)) > buf + bufsize) {
			errno = ERANGE;
			return NULL;
		}
		result->h_addr_list = (char **) ALIGN(bp, char *);
		memcpy(result->h_addr_list, addrs, (hap - addrs) * sizeof(char *));
		result->h_aliases = result->h_addr_list + (hap - addrs);
		memcpy(result->h_aliases, aliases, (ap - aliases) * sizeof(char *));
		return result;
	} else {
		*errval = TRY_AGAIN;
		return NULL;
	}
}

/* Performs global initialization. */
struct res_data *_res_init()
{
	struct res_data *data;
	
	/* Make sure the global initializations have been done. */
	pthread_once(&init_once, _res_init_global);
	if (init_status < 0)
		return NULL;
	
	/* Initialize thread-specific data for this thread if it hasn't
	 * been done already. */
	data = (struct res_data *) pthread_getspecific(key);
	if (!data) {
		data = (struct res_data *) malloc(sizeof(struct res_data));
		if (data == NULL)
			return NULL;
		if (pthread_setspecific(key, data) < 0) {
			free(data);
			return NULL;
		}
		data->buf = NULL;
		data->state = start;
		data->errval = NO_RECOVERY;
		data->sock = -1;
	}
	return data;
}

static void _res_init_global()
{
	int result;
	char line[BUFSIZ], buf[BUFSIZ], *domain, *p, *net;
	int i, localdomain_set = 0, num_servers = 0, num_sorts = 0;
	FILE *fp;
	struct in_addr addr;
	
	/* Assume an error state until we finish. */
	init_status = -1;
	
	/* Initialize the key for thread-specific data. */
	result = pthread_key_create(&key, free);
	if (result < 0)
		return;
	
	/* Initialize starting state. */
	start.retrans = RES_TIMEOUT;
	start.retry = DEFAULT_RETRIES;
	start.options = RES_DEFAULT;
	start.id = 0;
	start.nscount = 1;
	start.nsaddr.sin_addr.s_addr = INADDR_ANY;
	start.nsaddr.sin_family = AF_INET;
	start.nsaddr.sin_port = htons(NAMESERVER_PORT);
	start.nscount = 1;
	start.ndots = 1;
	start.pfcode = 0;
	strncpy(start.lookups, "f", sizeof(start.lookups));
	
	/* Look for a LOCALDOMAIN definition. */
	domain = getenv("LOCALDOMAIN");
	if (domain != NULL) {
		strncpy(start.defdname, domain, sizeof(start.defdname));
		domain = start.defdname;
		localdomain_set = 1;
		
		/* Construct a search path from the LOCALDOMAIN value, which is
		 * a space-separated list of strings.  For backwards-compatibility,
		 * a newline terminates the list. */
		i = 0;
		while (*domain && i < MAXDNSRCH) {
			start.dnsrch[i] = domain;
			while (*domain && !isspace(*domain))
				domain++;
			if (!*domain || *domain == '\n') {
				*domain = 0;
				break;
			}
			*domain++ = 0;
			while (isspace(*domain))
				domain++;
			i++;
		}
	}
	
	/* Look for a config file and read it in. */
	fp = fopen(_PATH_RESCONF, "r");
	if (fp != NULL) {
		strncpy(start.lookups, "bf", sizeof(start.lookups));
		
		/* Read in the configuration file. */
		while (fgets(line, sizeof(line), fp)) {
			
			/* Ignore blank lines and comments. */
			if (*line == ';' || *line == '#' || !*line)
				continue;
			
			if (strncmp(line, "domain", 6) == 0) {
				
				/* Read in the default domain, and initialize a one-
				 * element search path.	 Skip the domain line if we
				 * already got one from the LOCALDOMAIN environment
				 * variable. */
				if (localdomain_set)
					continue;
				
				/* Look for the next word in the line. */
				p = line + 6;
				while (*p == ' ' || *p == '\t')
					p++;
				if (!*p || *p == '\n')
					continue;
				
				/* Copy in the domain, and null-terminate it at the
				 * first tab or newline. */
				strncpy(start.defdname, p, sizeof(start.defdname) - 1);
				p = strpbrk(start.defdname, "\t\n");
				if (p)
					*p = 0;
				
				start.dnsrch[0] = start.defdname;
				start.dnsrch[1] = NULL;
				
			} else if (strncmp(line, "lookup", 6) == 0) {
				
				/* Get a list of lookup types. */
				memset(start.lookups, 0, sizeof(start.lookups));
				
				/* Find the next word in the line. */
				p = line + 6;
				while (isspace(*p))
					p++;
				
				i = 0;
				while (*p && i < MAXDNSLUS) {
					/* Add a lookup type. */
					if (*p == 'y' || *p == 'b' || *p == 'f')
						start.lookups[i++] = *p;
					
					/* Find the next word. */
					while (*p && !isspace(*p))
						p++;
					while (isspace(*p))
						p++;
				}
				
			} else if (strncmp(line, "search", 6) == 0) {
				
				/* Read in a space-separated list of domains to search
				 * when a name is not fully-qualified.	Skip this line
				 * if the LOCALDOMAIN environment variable was set. */
				if (localdomain_set)
					continue;
				
				/* Look for the next word on the line. */
				p = line + 6;
				while (*p == ' ' || *p == '\t')
					p++;
				if (!*p || *p == '\n')
					continue;
				
				/* Copy the rest of the line into start.defdname. */
				strncpy(start.defdname, p, sizeof(start.defdname) - 1);
				domain = start.defdname;
				p = strchr(domain, '\n');
				if (*p)
					*p = 0;
				
				/* Construct a search path from the line, which is a
				 * space-separated list of strings. */
				i = 0;
				while (*domain && i < MAXDNSRCH) {
					start.dnsrch[i] = domain;
					while (*domain && !isspace(*domain))
						domain++;
					if (!*domain || *domain == '\n') {
						*domain = 0;
						break;
					}
					*domain++ = 0;
					while (isspace(*domain))
						domain++;
					i++;
				}
				
			} else if (strncmp(line, "nameserver", 10) == 0) {
				
				/* Add an address to the list of name servers we can
				 * connect to. */
				
				/* Look for the next word in the line. */
				p = line + 10;
				while (*p == ' ' || *p == '\t')
					p++;
				if (*p && *p != '\n' && inet_aton(p, &addr)) {
					start.nsaddr_list[num_servers].sin_addr = addr;
					start.nsaddr_list[num_servers].sin_family = AF_INET;
					start.nsaddr_list[num_servers].sin_port =
						htons(NAMESERVER_PORT);
					if (++num_servers >= MAXNS)
					    break;
				}
				
			} else if (strncmp(line, "sortlist", 8) == 0) {
				
				p = line + 8;
				while (num_sorts < MAXRESOLVSORT) {
					
					/* Find the next word in the line. */
					p = line + 8;
					while (*p == ' ' || *p == '\t')
						p++;
					
					/* Read in an IP address and netmask. */
					if (sscanf(p, "%[0-9./]s", buf) != 1)
						break;
					net = strchr(buf, '/');
					if (net)
						*net = 0;
					
					/* Translate the address into an IP address
					 * and netmask. */
					if (inet_aton(buf, &addr)) {
						start.sort_list[num_sorts].addr = addr;
						if (net && inet_aton(net + 1, &addr)) {
							start.sort_list[num_sorts].mask = addr.s_addr;
						} else {
							start.sort_list[num_sorts].mask =
								net_mask(start.sort_list[num_sorts].addr);
						}
						num_sorts++;
					}
					
					/* Skip past this word. */
					if (net)
						*net = '/';
					p += strlen(buf);
				}
				
			}
		}
		fclose(fp);
	}
	
	/* If we don't have a default domain, strip off the first
	 * component of this machine's domain name, and make a one-
	 * element search path consisting of the default domain. */
	if (*start.defdname == 0) {
		if (gethostname(buf, sizeof(start.defdname) - 1) == 0) {
			p = strchr(buf, '.');
			if (p)
				strcpy(start.defdname, p + 1);
		}
		start.dnsrch[0] = start.defdname;
		start.dnsrch[1] = NULL;
	}
	
	p = getenv("RES_OPTIONS");
	if (p)
		set_options(p, "env");
	
	start.options |= RES_INIT;
	_res = start;
	init_status = 0;
}

static void set_options(const char *options, const char *source)
{
	const char *p = options;
	int i;
	
	while (*p) {
		
		/* Skip leading and inner runs of spaces. */
		while (*p == ' ' || *p == '\t')
			p++;
		
		/* Search for and process individual options. */
		if (strncmp(p, "ndots:", 6) == 0) {
			i = atoi(p + 6);
			start.ndots = (i <= RES_MAXNDOTS) ? i : RES_MAXNDOTS;
		} else if (!strncmp(p, "debug", 5))
		    start.options |= RES_DEBUG;
		else if (!strncmp(p, "usevc", 5))
		    start.options |= RES_USEVC;
		else if (!strncmp(p, "stayopen", 8))
		    start.options |= RES_STAYOPEN;

		/* Skip to next run of spaces */
		while (*p && *p != ' ' && *p != '\t')
			p++;
	}
}

static pthread_ipaddr_type net_mask(struct in_addr in)
{
	pthread_ipaddr_type i = ntohl(in.s_addr);
	
	if (IN_CLASSA(i))
		return htonl(IN_CLASSA_NET);
	if (IN_CLASSB(i))
		return htonl(IN_CLASSB_NET);
	return htonl(IN_CLASSC_NET);
}

/* Get the error value for this thread, or NO_RECOVERY if none has been
 * successfully set.  The screw case to worry about here is if
 * __res_init() fails for a resolver routine because it can't allocate
 * or set the thread-specific data, and then __res_init() succeeds here.
 * Because __res_init() sets errval to NO_RECOVERY after a successful
 * initialization, we return NO_RECOVERY in that case, which is correct. */
int _res_get_error()
{
	struct res_data *data;
	
	data = _res_init();
	return (data) ? data->errval : NO_RECOVERY;
}

struct __res_state *_res_status()
{
	struct res_data *data;
	
	data = _res_init();
	return (data) ? &data->state : NULL;
}

static int qcomp(const void *arg1, const void *arg2)
{
	const struct in_addr **a1 = (const struct in_addr **) arg1;
	const struct in_addr **a2 = (const struct in_addr **) arg2;
	struct __res_state *state = _res_status();
	
	int pos1, pos2;
	
	for (pos1 = 0; pos1 < state->nsort; pos1++) {
		if (state->sort_list[pos1].addr.s_addr ==
			((*a1)->s_addr & state->sort_list[pos1].mask))
			break;
	}
	for (pos2 = 0; pos2 < state->nsort; pos2++) {
		if (state->sort_list[pos2].addr.s_addr ==
			((*a2)->s_addr & state->sort_list[pos2].mask))
			break;
	}
	return pos1 - pos2;
}

/*
 * This routine is for closing the socket if a virtual circuit is used and
 * the program wants to close it.  We don't use this routine, but libc
 * might reference it.
 *
 * This routine is not expected to be user visible.
 */
void _res_close()
{
	struct res_data *data;

	data = _res_init();
	if (data && data->sock != -1) {
		(void) close(data->sock);
		data->sock = -1;
	}
}
