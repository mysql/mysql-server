/*-
 * Copyright (c) 1980, 1983, 1988 Regents of the University of California.
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
 *	from: @(#)netdb.h	5.15 (Berkeley) 4/3/91
 *	$Id$
 */

#ifndef _NETDB_H_
#define _NETDB_H_

#include <stdio.h>
#include <sys/types.h>
#include <netinet/in.h>

#define	_PATH_HEQUIV	"/etc/hosts.equiv"
#define	_PATH_HOSTS	"/etc/hosts"
#define	_PATH_NETWORKS	"/etc/networks"
#define	_PATH_PROTOCOLS	"/etc/protocols"
#define	_PATH_SERVICES	"/etc/services"
#define __NETDB_MAXALIASES	35
#define __NETDB_MAXADDRS	35

/*
 * Structures returned by network data base library.  All addresses are
 * supplied in host order, and returned in network order (suitable for
 * use in system calls).
 */
struct	hostent {
	char	*h_name;	/* official name of host */
	char	**h_aliases;	/* alias list */
	int	h_addrtype;	/* host address type */
	int	h_length;	/* length of address */
	char	**h_addr_list;	/* list of addresses from name server */
#define	h_addr	h_addr_list[0]	/* address, for backward compatiblity */
};

/*
 * Assumption here is that a network number
 * fits in 32 bits -- probably a poor one.
 */
struct	netent {
	char		*n_name;	/* official name of net */
	char		**n_aliases;	/* alias list */
	int		n_addrtype;	/* net address type */
	unsigned long	n_net;		/* network # */
};

struct	servent {
	char	*s_name;	/* official service name */
	char	**s_aliases;	/* alias list */
	int	s_port;		/* port # */
	char	*s_proto;	/* protocol to use */
};

struct	protoent {
	char	*p_name;	/* official protocol name */
	char	**p_aliases;	/* alias list */
	int	p_proto;	/* protocol # */
};

/*
 * Error return codes from gethostbyname() and gethostbyaddr()
 * (left in extern int h_errno).
 */

#define	HOST_NOT_FOUND	1 /* Authoritative Answer Host not found */
#define	TRY_AGAIN	2 /* Non-Authoritive Host not found, or SERVERFAIL */
#define	NO_RECOVERY	3 /* Non recoverable errors, FORMERR, REFUSED, NOTIMP */
#define	NO_DATA		4 /* Valid name, no data record of requested type */
#define	NO_ADDRESS	NO_DATA		/* no address, look for MX record */

#include <sys/cdefs.h>

__BEGIN_DECLS
void		endhostent __P_((void));
void		endnetent __P_((void));
void		endprotoent __P_((void));
void		endservent __P_((void));
struct hostent	*gethostbyaddr __P_((const char *, int, int));
struct hostent	*gethostbyname __P_((const char *));
struct hostent	*gethostent __P_((void));
struct netent	*getnetbyaddr __P_((long, int)); /* u_long? */
struct netent	*getnetbyname __P_((const char *));
struct netent	*getnetent __P_((void));
struct protoent	*getprotobyname __P_((const char *));
struct protoent	*getprotobynumber __P_((int));
struct protoent	*getprotoent __P_((void));
struct servent	*getservbyname __P_((const char *, const char *));
struct servent	*getservbyport __P_((int, const char *));
struct servent	*getservent __P_((void));
void		herror __P_((const char *));
char		*hstrerror __P_((int));
void		sethostent __P_((int));
void		setnetent __P_((int));
void		setprotoent __P_((int));
void		setservent __P_((int));
struct hostent	*gethostbyaddr_r __P_((const char *, int, int,
				      struct hostent *, char *, int, int *));
struct hostent	*gethostbyname_r __P_((const char *, struct hostent *, char *,
				      int, int *));
struct hostent	*gethostent_r __P_((struct hostent *, char *, int, int *));
struct netent	*getnetbyaddr_r __P_((long, int, struct netent *, char *, int));
struct netent	*getnetbyname_r __P_((const char *, struct netent *, char *,
				     int));
struct netent	*getnetent_r __P_((struct netent *, char *, int));
struct protoent	*getprotobyname_r __P_((const char *, struct protoent *, char *,
				       int));
struct protoent	*getprotobynumber_r __P_((int, struct protoent *, char *, int));
struct protoent	*getprotoent_r __P_((struct protoent *, char *, int));
struct servent	*getservbyname_r __P_((const char *, const char *,
				      struct servent *, char *, int));
struct servent	*getservbyport_r __P_((int, const char *, struct servent *,
				      char *, int));
struct servent	*getservent_r __P_((struct servent *, char *, int));
__END_DECLS

#endif /* !_NETDB_H_ */
