/*
 * Copyright (c) 1983, 1987, 1989 The Regents of the University of California.
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
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
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
 *	from: @(#)resolv.h	5.15 (Berkeley) 4/3/91
 *	$Id$
 */

#ifndef _RESOLV_H_
#define	_RESOLV_H_

#include <netinet/in.h> 
/*
 * This is specificly for Solaris which defines NOERROR in the streams
 * header files and defines it differently than in arpa/nameser.h
 */
#ifdef NOERROR
#undef NOERROR
#endif
#include <arpa/nameser.h>

/*
 * revision information.  this is the release date in YYYYMMDD format.
 * it can change every day so the right thing to do with it is use it
 * in preprocessor commands such as "#if (__RES > 19931104)".  do not
 * compare for equality; rather, use it to determine whether your resolver
 * is new enough to contain a certain feature.
 */

#define	__RES		19940703

/*
 * Resolver configuration file.
 * Normally not present, but may contain the address of the
 * inital name server(s) to query and the domain search list.
 */

#ifndef _PATH_RESCONF
#define _PATH_RESCONF        "/etc/resolv.conf"
#endif

/*
 * Global defines and variables for resolver stub.
 */
#define	MAXNS			3	/* max # name servers we'll track */
#define	MAXDFLSRCH		3	/* # default domain levels to try */
#define	MAXDNSRCH		6	/* max # domains in search path */
#define	LOCALDOMAINPARTS	2	/* min levels in name that is "local" */
#define	MAXDNSLUS		4	/* max # of host lookup types */

#define	RES_TIMEOUT		5	/* min. seconds between retries */
#define MAXRESOLVSORT		10	/* number of net to sort on */
#define RES_MAXNDOTS		15	/* should reflect bit field size */

struct __res_state {
	int	retrans;	 	/* retransmition time interval */
	int	retry;			/* number of times to retransmit */
	long	options;		/* option flags - see below. */
	int	nscount;		/* number of name servers */
	struct	sockaddr_in nsaddr_list[MAXNS];	/* address of name server */
#define	nsaddr	nsaddr_list[0]		/* for backward compatibility */
	u_short	id;			/* current packet id */
	char	*dnsrch[MAXDNSRCH+1];	/* components of domain to search */
	char	defdname[MAXDNAME];	/* default domain */
	long	pfcode;			/* RES_PRF_ flags - see below. */
	u_char	ndots:4;		/* threshold for initial abs. query */
	u_char	nsort:4;		/* number of elements in sort_list[] */
	char	unused[3];
	struct {
		struct in_addr addr;
		u_long mask;
	} sort_list[MAXRESOLVSORT];
	char	lookups[MAXDNSLUS];
};

/*
 * Resolver options
 */
#define RES_INIT	0x0001		/* address initialized */
#define RES_DEBUG	0x0002		/* print debug messages */
#define RES_AAONLY	0x0004		/* authoritative answers only */
#define RES_USEVC	0x0008		/* use virtual circuit */
#define RES_PRIMARY	0x0010		/* query primary server only */
#define RES_IGNTC	0x0020		/* ignore trucation errors */
#define RES_RECURSE	0x0040		/* recursion desired */
#define RES_DEFNAMES	0x0080		/* use default domain name */
#define RES_STAYOPEN	0x0100		/* Keep TCP socket open */
#define RES_DNSRCH	0x0200		/* search up local domain tree */

#define RES_DEFAULT	(RES_RECURSE | RES_DEFNAMES | RES_DNSRCH)

/*
 * Resolver "pfcode" values.  Used by dig.
 */
#define	RES_PRF_STATS	0x0001
/*			0x0002  */
#define	RES_PRF_CLASS	0x0004
#define	RES_PRF_CMD	0x0008
#define	RES_PRF_QUES	0x0010
#define	RES_PRF_ANS	0x0020
#define	RES_PRF_AUTH	0x0040
#define	RES_PRF_ADD	0x0080
#define	RES_PRF_HEAD1	0x0100
#define	RES_PRF_HEAD2	0x0200
#define	RES_PRF_TTLID	0x0400
#define	RES_PRF_HEADX	0x0800
#define	RES_PRF_QUERY	0x1000
#define	RES_PRF_REPLY	0x2000
#define	RES_PRF_INIT	0x4000
/*			0x8000  */

#define _res		(*_res_status())
#define h_errno		(_res_get_error())

#include <sys/cdefs.h>
#include <stdio.h>

/* Private routines shared between libc/net, named, nslookup and others. */
#define	dn_skipname	__dn_skipname
#define	fp_query	__fp_query
#define	hostalias	__hostalias
#define	putlong		__putlong
#define	putshort	__putshort
#define p_class		__p_class
#define p_time		__p_time
#define p_type		__p_type
__BEGIN_DECLS
struct __res_state *_res_status __P_((void));
int _res_get_error __P_((void));

int	 __dn_skipname __P_((const u_char *, const u_char *));
void	 __fp_query __P_((char *, FILE *));
char	*__hostalias __P_((const char *));
void	 __putlong __P_((pthread_ipaddr_type, unsigned char *));
void	 __putshort __P_((pthread_ipport_type, unsigned char *));
char	*__p_class __P_((int));
char	*__p_time __P_((unsigned long));
char	*__p_type __P_((int));

int	 dn_comp __P_((const unsigned char *, unsigned char *, int,
		      unsigned char **, unsigned char **));
int	 dn_expand __P_((const unsigned char *, const unsigned char *,
			const unsigned char *, unsigned char *, int));
int	 res_init __P_((void));
int	 res_mkquery __P_((int, const char *, int, int, const char *, int,
			  const char *, char *, int));
int	 res_send __P_((const char *, int, char *, int));
__END_DECLS

#endif /* !_RESOLV_H_ */

