/* ==== socket.h.h ============================================================
 * Copyright (c) 1994 by Chris Provenzano, proven@athena.mit.edu	
 *
 * Description : Correct Linux header file.
 */

#ifndef _PTHREAD_SOCKET_H_
#define _PTHREAD_SOCKET_H_

/* #include <linux/socket.h> */
#ifndef _LINUX_SOCKET_H
#define _LINUX_SOCKET_H

/* IP options */
#define IP_TOS				1
#define	IPTOS_LOWDELAY		0x10
#define	IPTOS_THROUGHPUT	0x08
#define	IPTOS_RELIABILITY	0x04
#define IP_TTL				2
#ifndef IP_HDRINCL
#define IP_HDRINCL			3
#endif
#ifdef V1_3_WILL_DO_THIS_FUNKY_STUFF
#define IP_OPTIONS			4
#endif

#endif

/* #include <asm/socket.h>				arch-dependent defines	*/
#include <linux/sockios.h>			/* the SIOCxxx I/O controls	*/
#include <pthread/posix.h>

struct sockaddr {
  	unsigned short	sa_family;		/* address family, AF_xxx	*/
  	char			sa_data[14];	/* 14 bytes of protocol address	*/
};

struct linger {
  	int 			l_onoff;		/* Linger active		*/
  	int				l_linger;		/* How long to linger for	*/
};

struct msghdr 
{
	void	*		msg_name;		/* Socket name			*/
	int				msg_namelen;	/* Length of name		*/
	struct iovec *	msg_iov;		/* Data blocks			*/
	int 			msg_iovlen;		/* Number of blocks		*/
	void 	*		msg_accrights;	/* Per protocol magic (eg BSD file descriptor passing) */
	int				msg_accrightslen;/* Length of rights list */
};

/* Socket types. */
#define SOCK_STREAM	1				/* stream (connection) socket	*/
#define SOCK_DGRAM	2				/* datagram (conn.less) socket	*/
#define SOCK_RAW	3				/* raw socket			*/
#define SOCK_RDM	4				/* reliably-delivered message	*/
#define SOCK_SEQPACKET	5			/* sequential packet socket	*/
#define SOCK_PACKET	10				/* linux specific way of	*/
									/* getting packets at the dev	*/
									/* level.  For writing rarp and	*/
									/* other similar things on the	*/
									/* user level.			*/

/* Supported address families. */
#define AF_UNSPEC	0
#define AF_UNIX		1				/* Unix domain sockets 		*/
#define AF_INET		2				/* Internet IP Protocol 	*/
#define AF_AX25		3				/* Amateur Radio AX.25 		*/
#define AF_IPX		4				/* Novell IPX 			*/
#define AF_APPLETALK 5				/* Appletalk DDP 		*/
#define	AF_NETROM	6				/* Amateur radio NetROM 	*/
#define AF_BRIDGE	7				/* Multiprotocol bridge 	*/
#define AF_AAL5		8				/* Reserved for Werner's ATM 	*/
#define AF_X25		9				/* Reserved for X.25 project 	*/
#define AF_INET6	10				/* IP version 6			*/
#define AF_MAX		12				/* For now.. */

/* Protocol families, same as address families. */
#define PF_UNSPEC	AF_UNSPEC
#define PF_UNIX		AF_UNIX
#define PF_INET		AF_INET
#define PF_AX25		AF_AX25
#define PF_IPX		AF_IPX
#define PF_APPLETALK AF_APPLETALK
#define	PF_NETROM	AF_NETROM
#define PF_BRIDGE	AF_BRIDGE
#define PF_AAL5		AF_AAL5
#define PF_X25		AF_X25
#define PF_INET6	AF_INET6

#define PF_MAX		AF_MAX

/* Maximum queue length specificable by listen.  */
#define SOMAXCONN	128

/* Flags we can use with send/ and recv. */
#define MSG_OOB		1
#define MSG_PEEK	2
#define MSG_DONTROUTE	4

/* Setsockoptions(2) level. Thanks to BSD these must match IPPROTO_xxx */
#define SOL_SOCKET	1
#define SOL_IP		0
#define SOL_IPX		256
#define SOL_AX25	257
#define SOL_ATALK	258
#define	SOL_NETROM	259
#define SOL_TCP		6
#define SOL_UDP		17

/* For setsockoptions(2) */
#define SO_DEBUG	1
#define SO_REUSEADDR	2
#define SO_TYPE		3
#define SO_ERROR	4
#define SO_DONTROUTE	5
#define SO_BROADCAST	6
#define SO_SNDBUF	7
#define SO_RCVBUF	8
#define SO_KEEPALIVE	9
#define SO_OOBINLINE	10
#define SO_NO_CHECK	11
#define SO_PRIORITY	12
#define SO_LINGER	13
/* To add :#define SO_REUSEPORT 14 */


#define IP_MULTICAST_IF			32
#define IP_MULTICAST_TTL 		33
#define IP_MULTICAST_LOOP 		34
#define IP_ADD_MEMBERSHIP		35
#define IP_DROP_MEMBERSHIP		36


/* These need to appear somewhere around here */
#define IP_DEFAULT_MULTICAST_TTL        1
#define IP_DEFAULT_MULTICAST_LOOP       1
#define IP_MAX_MEMBERSHIPS              20
 
/* IPX options */
#define IPX_TYPE	1

/* TCP options - this way around because someone left a set in the c library includes */
#define TCP_NODELAY	1
#define TCP_MAXSEG	2

/* The various priorities. */
#define SOPRI_INTERACTIVE	0
#define SOPRI_NORMAL		1
#define SOPRI_BACKGROUND	2

/* 
 * Functions
 */

__BEGIN_DECLS

int			accept			__P_((int, struct sockaddr *, int *));
int			bind			__P_((int, const struct sockaddr *, int));
int			connect			__P_((int, const struct sockaddr *, int));
int			listen			__P_((int, int));
int			socket			__P_((int, int, int));

int getsockopt __P_((int __s, int __level, int __optname,
                void *__optval, int *__optlen));
int setsockopt __P_((int __s, int __level, int __optname,
                __const void *__optval, int optlen));
int getsockname __P_((int __sockfd, struct sockaddr *__addr,
                int *__paddrlen));
int getpeername __P_((int __sockfd, struct sockaddr *__peer,
                int *__paddrlen));
ssize_t send __P_((int __sockfd, __const void *__buff, size_t __len, int __flags));
ssize_t recv __P_((int __sockfd, void *__buff, size_t __len, int __flags));
ssize_t sendto __P_((int __sockfd, __const void *__buff, size_t __len,
                 int __flags, __const struct sockaddr *__to,
                 int __tolen));
ssize_t recvfrom __P_((int __sockfd, void *__buff, size_t __len,
                 int __flags, struct sockaddr *__from,
                 int *__fromlen));
extern ssize_t sendmsg __P_((int __fd, __const struct msghdr *__message,
                        int __flags));
extern ssize_t recvmsg __P_((int __fd, struct msghdr *__message,
                        int __flags));
int shutdown __P_((int __sockfd, int __how));

__END_DECLS

#endif




