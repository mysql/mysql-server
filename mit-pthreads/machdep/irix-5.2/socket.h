#ifndef __SYS_TPI_SOCKET_H__
#ifndef __SYS_SOCKET_H__
#define __SYS_SOCKET_H__
/*
 * Copyright (c) 1982,1985, 1986 Regents of the University of California.
 * All rights reserved.  The Berkeley software License Agreement
 * specifies the terms and conditions for redistribution.
 *
 *	@(#)socket.h	7.1 (Berkeley) 6/4/86
 */
#include <sys/cdefs.h>
#include <sys/bsd_types.h>

/*
 * Definitions related to sockets: types, address families, options.
 */

/*
 * Types
 */
#ifdef _STYPES_LATER	/* old ABI */
#define	SOCK_STREAM	1		/* stream socket */
#define	SOCK_DGRAM	2		/* datagram socket */
#define	SOCK_RAW	3		/* raw-protocol interface */
#define	SOCK_RDM	4		/* reliably-delivered message */
#define	SOCK_SEQPACKET	5		/* sequenced packet stream */
#else /* !_STYPES_LATER, new ABI */

#ifndef NC_TPI_CLTS
#define NC_TPI_CLTS	1		/* must agree with netconfig.h */
#define NC_TPI_COTS	2		/* must agree with netconfig.h */
#define NC_TPI_COTS_ORD	3		/* must agree with netconfig.h */
#define	NC_TPI_RAW	4		/* must agree with netconfig.h */
#endif /* !NC_TPI_CLTS */

#define	SOCK_DGRAM	NC_TPI_CLTS	/* datagram socket */
#define	SOCK_STREAM	NC_TPI_COTS	/* stream socket */
#define	SOCK_RAW	NC_TPI_RAW	/* raw-protocol interface */
#define	SOCK_RDM	5		/* reliably-delivered message */
#define	SOCK_SEQPACKET	6		/* sequenced packet stream */

#ifdef _KERNEL
#define	IRIX4_SOCK_STREAM	1	/* stream socket */
#define	IRIX4_SOCK_DGRAM	2	/* datagram socket */
#define	IRIX4_SOCK_RAW		3	/* raw-protocol interface */
#define	IRIX4_SOCK_RDM		4	/* reliably-delivered message */
#define	IRIX4_SOCK_SEQPACKET	5	/* sequenced packet stream */
#endif /* _KERNEL */
#endif /* _STYPES_LATER */

/*
 * Option flags per-socket.
 */
#define	SO_DEBUG	0x0001		/* turn on debugging info recording */
#define	SO_ACCEPTCONN	0x0002		/* socket has had listen() */
#define	SO_REUSEADDR	0x0004		/* allow local address reuse */
#define	SO_KEEPALIVE	0x0008		/* keep connections alive */
#define	SO_DONTROUTE	0x0010		/* just use interface addresses */
#define	SO_BROADCAST	0x0020		/* permit sending of broadcast msgs */
#define	SO_USELOOPBACK	0x0040		/* bypass hardware when possible */
#define	SO_LINGER	0x0080		/* linger on close if data present */
#define	SO_OOBINLINE	0x0100		/* leave received OOB data in line */
#define	SO_REUSEPORT	0x0200		/* allow local address,port reuse */
#define SO_ORDREL	0x0200		/* MIPS ABI - unimplemented */
#define SO_IMASOCKET	0x0400		/* use libsocket (not TLI) semantics */
#define	SO_CHAMELEON	0x1000		/* (cipso) set label to 1st req rcvd */

/*
 * Additional options, not kept in so_options.
 */
#define SO_SNDBUF	0x1001		/* send buffer size */
#define SO_RCVBUF	0x1002		/* receive buffer size */
#define SO_SNDLOWAT	0x1003		/* send low-water mark */
#define SO_RCVLOWAT	0x1004		/* receive low-water mark */
#define SO_SNDTIMEO	0x1005		/* send timeout */
#define SO_RCVTIMEO	0x1006		/* receive timeout */
#define	SO_ERROR	0x1007		/* get error status and clear */
#define	SO_TYPE		0x1008		/* get socket type */
#define SO_PROTOTYPE	0x1009		/* get protocol type (libsocket) */

/*
 * Structure used for manipulating linger option.
 */
struct	linger {
	int	l_onoff;		/* option on/off */
	int	l_linger;		/* linger time */
};

/*
 * Level number for (get/set)sockopt() to apply to socket itself.
 */
#define	SOL_SOCKET	0xffff		/* options for socket level */

/*
 * Address families.
 * XTP really is not an address family, but is included here to take
 * up space, because other AF_ entries are numerically equal to their
 * PF_ counterparts.
 */
#define	AF_UNSPEC	0		/* unspecified */
#define	AF_UNIX		1		/* local to host (pipes, portals) */
#define	AF_INET		2		/* internetwork: UDP, TCP, etc. */
#define	AF_IMPLINK	3		/* arpanet imp addresses */
#define	AF_PUP		4		/* pup protocols: e.g. BSP */
#define	AF_CHAOS	5		/* mit CHAOS protocols */
#define	AF_NS		6		/* XEROX NS protocols */
#define	AF_ISO		7		/* ISO protocols */
#define	AF_ECMA		8		/* european computer manufacturers */
#define	AF_DATAKIT	9		/* datakit protocols */
#define	AF_CCITT	10		/* CCITT protocols, X.25 etc */
#define	AF_SNA		11		/* IBM SNA */
#define AF_DECnet	12		/* DECnet */
#define AF_DLI		13		/* DEC Direct data link interface */
#define AF_LAT		14		/* LAT */
#define	AF_HYLINK	15		/* NSC Hyperchannel */
#define	AF_APPLETALK	16		/* Apple Talk */
#define	AF_ROUTE	17		/* Internal Routing Protocol */
#ifdef __sgi
#define	AF_RAW		18		/* Raw link layer interface */
#else
#define	AF_LINK		18		/* Link layer interface */
#endif
#define	pseudo_AF_XTP	19		/* eXpress Transfer Protocol (no AF) */

/* MIPS ABI VALUES - unimplemented */
#define AF_NIT		17		/* Network Interface Tap */
#define AF_802		18		/* IEEE 802.2, also ISO 8802 */
#define AF_OSI		19		/* umbrella for all families used */
#define AF_X25		20		/* CCITT X.25 in particular */
#define AF_OSINET	21		/* AFI = 47, IDI = 4 */
#define AF_GOSIP	22		/* U.S. Government OSI */


#define AF_SDL		23		/* SGI Data Link for DLPI */

#define AF_MAX		(AF_SDL+1)

/*
 * Structure used by kernel to store most
 * addresses.
 */
struct sockaddr {
	u_short	sa_family;		/* address family */
	char	sa_data[14];		/* up to 14 bytes of direct address */
};

/*
 * Structure used by kernel to pass protocol
 * information in raw sockets.
 */
struct sockproto {
	u_short	sp_family;		/* address family */
	u_short	sp_protocol;		/* protocol */
};

/*
 * An option specification consists of an opthdr, followed by the value of
 * the option.  An options buffer contains one or more options.  The len
 * field of opthdr specifies the length of the option value in bytes.  This
 * length must be a multiple of sizeof(long) (use OPTLEN macro).
 */

struct opthdr {
        long            level;  /* protocol level affected */
        long            name;   /* option to modify */
        long            len;    /* length of option value */
};

#define OPTLEN(x) ((((x) + sizeof(long) - 1) / sizeof(long)) * sizeof(long))
#define OPTVAL(opt) ((char *)(opt + 1))

/*
 * the optdefault structure is used for internal tables of option default
 * values.
 */
struct optdefault {
	int	optname;	/* the option */
	char	*val;		/* ptr to default value */
	int	len;		/* length of value */
};

struct tpisocket;
struct T_optmgmt_req;
struct msgb;

/*
 * the opproc structure is used to build tables of options processing
 * functions for dooptions().
 */
struct opproc {
	int	level;  	/* options level this function handles */
	int	(*func)(struct tpisocket *, struct T_optmgmt_req *,
			struct opthdr *, struct msgb *);
				/* the function */
};

/*
 * This structure is used to encode pseudo system calls
 */
struct socksysreq {
	int             args[7];
};

/*
 * This structure is used for adding new protocols to the list supported by
 * sockets.
 */

struct socknewproto {
	int             family;	/* address family (AF_INET, etc.) */
	int             type;	/* protocol type (SOCK_STREAM, etc.) */
	int             proto;	/* per family proto number */
	dev_t           dev;	/* major/minor to use (must be a clone) */
	int             flags;	/* protosw flags */
};

/*
 * Protocol families, same as address families for now.
 */
#define	PF_UNSPEC	AF_UNSPEC
#define	PF_UNIX		AF_UNIX
#define	PF_INET		AF_INET
#define	PF_IMPLINK	AF_IMPLINK
#define	PF_PUP		AF_PUP
#define	PF_CHAOS	AF_CHAOS
#define	PF_NS		AF_NS
#define	PF_ISO		AF_ISO
#define	PF_ECMA		AF_ECMA
#define	PF_DATAKIT	AF_DATAKIT
#define	PF_CCITT	AF_CCITT
#define	PF_SNA		AF_SNA
#define PF_DECnet	AF_DECnet
#define PF_DLI		AF_DLI
#define PF_LAT		AF_LAT
#define	PF_HYLINK	AF_HYLINK
#define	PF_APPLETALK	AF_APPLETALK
#define	PF_ROUTE	AF_ROUTE
#define	PF_LINK		AF_LINK
#define	PF_XTP		pseudo_AF_XTP	/* really just proto family, no AF */
#ifdef __sgi
#define	PF_RAW		AF_RAW
#endif

/* MIPS ABI VALUES - unimplemented */
#define PF_NIT		AF_NIT		/* Network Interface Tap */
#define PF_802		AF_802		/* IEEE 802.2, also ISO 8802 */
#define PF_OSI		AF_OSI		/* umbrella for all families used */
#define PF_X25		AF_X25		/* CCITT X.25 in particular */
#define PF_OSINET	AF_OSINET	/* AFI = 47, IDI = 4 */
#define PF_GOSIP	AF_GOSIP	/* U.S. Government OSI */

#define	PF_MAX		AF_MAX

/*
 * Maximum queue length specifiable by listen.
 */
#define	SOMAXCONN	5

/*
 * Message header for recvmsg and sendmsg calls.
 */
struct msghdr {
	caddr_t	msg_name;		/* optional address */
	int	msg_namelen;		/* size of address */
	struct	iovec *msg_iov;		/* scatter/gather array */
	int	msg_iovlen;		/* # elements in msg_iov */
	caddr_t	msg_accrights;		/* access rights sent/received */
	int	msg_accrightslen;
};

#define	MSG_OOB		0x1		/* process out-of-band data */
#define	MSG_PEEK	0x2		/* peek at incoming message */
#define	MSG_DONTROUTE	0x4	/* send without using routing tables */
#define MSG_EOR		0x8		/* data completes record (OSI only) */
#ifdef XTP
#define	MSG_BTAG	0x40	/* XTP packet with BTAG field */
#define	MSG_ETAG	0x80	/* XTP packet with ETAG field */
#endif

#define	MSG_MAXIOVLEN	16

__BEGIN_DECLS
int accept          __P_((int, struct sockaddr *, int *));
int bind            __P_((int, const struct sockaddr *, int));
int connect         __P_((int, const struct sockaddr *, int));
int getpeername     __P_((int, struct sockaddr *, int *));
int getsockname     __P_((int, struct sockaddr *, int *));
int getsockopt      __P_((int, int, int, void *, int *));
int listen          __P_((int, int));
ssize_t recv        __P_((int, void *, size_t, int));
ssize_t recvfrom    __P_((int, void *, size_t, int, struct sockaddr *, int *));
int recvmsg         __P_((int, struct msghdr *, int));
ssize_t send        __P_((int, const void *, size_t, int));
ssize_t sendto      __P_((int, const void *, size_t, int, 
                        const struct sockaddr *, int));
int sendmsg         __P_((int, const struct msghdr *, int));
int setsockopt      __P_((int, int, int, const void *, int));
int shutdown        __P_((int, int));
int socket          __P_((int, int, int));
int socketpair      __P_((int, int, int, int *));
__END_DECLS

#endif /* !__SYS_SOCKET_H__ */
#endif /* !__SYS_TPI_SOCKET_H__ */
