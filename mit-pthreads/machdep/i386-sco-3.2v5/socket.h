/*      @(#)socket.h	6.23 7/18/94 - STREAMware TCP/IP  source        */
/*
 * Copyrighted as an unpublished work.
 * (c) Copyright 1987-1994 Legent Corporation
 * All rights reserved.
 *
 * RESTRICTED RIGHTS
 *
 * These programs are supplied under a license.  They may be used,
 * disclosed, and/or copied only as permitted under such license
 * agreement.  Any copy must contain the above copyright notice and
 * this restricted rights notice.  Use, copying, and/or disclosure
 * of the programs is strictly prohibited unless otherwise provided
 * in the license agreement.
 *
 */
/*      SCCS IDENTIFICATION        */
/*
 * Copyright (c) 1985 Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms are permitted
 * provided that the above copyright notice and this paragraph are
 * duplicated in all such forms and that any documentation,
 * advertising materials, and other materials related to such
 * distribution and use acknowledge that the software was developed
 * by the University of California, Berkeley.  The name of the
 * University may not be used to endorse or promote products derived
 * from this software without specific prior written permission.
 * THIS SOFTWARE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTIBILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#ifndef __sys_socket_h
#define __sys_socket_h

#if !defined(FD_SETSIZE)
/* Pick up select stuff from standard system include */
#include <sys/types.h>
#endif

/* socket.h	6.1	83/07/29	 */

/*
 * Definitions related to sockets: types, address families, options.
 */

/*
 * Types
 */
#define	SOCK_STREAM	1	/* stream socket */
#define	SOCK_DGRAM	2	/* datagram socket */
#define	SOCK_RAW	3	/* raw-protocol interface */
#define	SOCK_RDM	4	/* reliably-delivered message */
#define	SOCK_SEQPACKET	5	/* sequenced packet stream */

/*
 * Option flags per-socket.
 */
#define	SO_DEBUG	0x0001	/* turn on debugging info recording */
#define	SO_ACCEPTCONN	0x0002	/* socket has had listen() */
#define	SO_REUSEADDR	0x0004	/* allow local address reuse */
#define	SO_KEEPALIVE	0x0008	/* keep connections alive */
#define	SO_DONTROUTE	0x0010	/* just use interface addresses */
#define	SO_BROADCAST	0x0020	/* permit sending of broadcast msgs */
#define	SO_USELOOPBACK	0x0040	/* bypass hardware when possible */
#define	SO_LINGER	0x0080	/* linger on close if data present */
#define	SO_OOBINLINE	0x0100	/* leave received OOB data in line */
#define SO_ORDREL	0x0200	/* give use orderly release */
#define SO_IMASOCKET	0x0400	/* use socket semantics (affects bind) */
#define SO_MGMT		0x0800	/* => it is used for mgmt. purposes */
#define	SO_REUSEPORT	0x1000	/* allow local port reuse */

/*
 * Additional options, not kept in so_options.
 */
#define SO_SNDBUF	0x1001	/* send buffer size */
#define SO_RCVBUF	0x1002	/* receive buffer size */
#define SO_SNDLOWAT	0x1003	/* send low-water mark */
#define SO_RCVLOWAT	0x1004	/* receive low-water mark */
#define SO_SNDTIMEO	0x1005	/* send timeout */
#define SO_RCVTIMEO	0x1006	/* receive timeout */
#define	SO_ERROR	0x1007	/* get error status and clear */
#define	SO_TYPE		0x1008	/* get socket type */
#define SO_PROTOTYPE	0x1009	/* get/set protocol type */

/*
 * Structure used for manipulating linger option.
 */
struct linger {
	int             l_onoff;	/* option on/off */
	int             l_linger;	/* linger time */
};

/*
 * Level number for (get/set)sockopt() to apply to socket itself.
 */
#define	SOL_SOCKET	0xffff	/* options for socket level */

/*
 * An option specification consists of an opthdr, followed by the value of
 * the option.  An options buffer contains one or more options.  The len
 * field of opthdr specifies the length of the option value in bytes.  This
 * length must be a multiple of sizeof(long) (use OPTLEN macro).
 */

struct opthdr {
	long            level;	/* protocol level affected */
	long            name;	/* option to modify */
	long            len;	/* length of option value */
};

#define OPTLEN(x) ((((x) + sizeof(long) - 1) / sizeof(long)) * sizeof(long))
#define OPTVAL(opt) ((char *)(opt + 1))

#if defined(INKERNEL) || defined(_KERNEL) || defined(_INKERNEL)
/*
 * the optdefault structure is used for internal tables of option default
 * values.
 */
struct optdefault {
	int             optname;/* the option */
	char           *val;	/* ptr to default value */
	int             len;	/* length of value */
};

/*
 * the opproc structure is used to build tables of options processing
 * functions for in_dooptions().
 */
struct opproc {
	int             level;	/* options level this function handles */
	int             (*func) ();	/* the function */
};
#endif

/*
 * Address families.
 */
#define	AF_UNSPEC	0	/* unspecified */
#define	AF_UNIX		1	/* local to host (pipes, portals) */
#define	AF_INET		2	/* internetwork: UDP, TCP, etc. */
#define	AF_IMPLINK	3	/* arpanet imp addresses */
#define	AF_PUP		4	/* pup protocols: e.g. BSP */
#define	AF_CHAOS	5	/* mit CHAOS protocols */
#define	AF_NS		6	/* XEROX NS protocols */
#define	AF_ISO		7	/* ISO protocols */
#define	AF_OSI		AF_ISO
#define	AF_ECMA		8	/* european computer manufacturers */
#define	AF_DATAKIT	9	/* datakit protocols */
#define	AF_CCITT	10	/* CCITT protocols, X.25 etc */
#define	AF_SNA		11	/* IBM SNA */
#define AF_DECnet	12	/* DECnet */
#define AF_DLI		13	/* Direct data link interface */
#define AF_LAT		14	/* LAT */
#define	AF_HYLINK	15	/* NSC Hyperchannel */
#define	AF_APPLETALK	16	/* Apple Talk */
#define AF_ROUTE        17      /* Internal Routing Protocol */
#define AF_LINK         18      /* Link layer interface */
#define pseudo_AF_XTP   19      /* eXpress Transfer Protocol (no AF) */

#define AF_MAX          20

/*
 * Structure used by kernel to store most addresses.
 */
struct sockaddr {
	u_short         sa_family;	/* address family */
	char            sa_data[14];	/* up to 14 bytes of direct address */
};

/*
 * Structure used by kernel to pass protocol information in raw sockets.
 */
struct sockproto {
	unsigned short  sp_family;	/* address family */
	unsigned short  sp_protocol;	/* protocol */
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
#define	PF_NBS		AF_NBS
#define	PF_ECMA		AF_ECMA
#define	PF_DATAKIT	AF_DATAKIT
#define	PF_CCITT	AF_CCITT
#define	PF_SNA		AF_SNA
#define PF_DECnet       AF_DECnet
#define PF_DLI          AF_DLI
#define PF_LAT          AF_LAT
#define PF_HYLINK       AF_HYLINK
#define PF_APPLETALK    AF_APPLETALK
#define PF_ROUTE        AF_ROUTE
#define PF_LINK         AF_LINK
#define PF_XTP          pseudo_AF_XTP   /* really just proto family, no AF */

#define PF_MAX          AF_MAX

/*
 * Maximum queue length specifiable by listen.
 */
#define SOMAXCONN       5

/*
 * Message header for recvmsg and sendmsg calls.
 * Used value-result for recmvsg, value only for sendmsg.
 */
struct msghdr {
	caddr_t         msg_name;	/* optional address */
	u_int           msg_namelen;	/* size of address */
	struct iovec   *msg_iov;	/* scatter/gather array */
	u_int           msg_iovlen;	/* # elements msg_iov */
	caddr_t         msg_control;	/* ancillary data, see below */
	u_int           msg_controllen;	/* ancillary data buffer len */
	int		msg_flags;	/* flags on received message */
};
#define msg_accrights		msg_control
#define	msg_accrightslen	msg_controllen

#define	MSG_OOB		0x1	/* process out-of-band data */
#define	MSG_PEEK	0x2	/* peek at incoming message */
#define	MSG_DONTROUTE	0x4	/* send without using routing tables */
#define MSG_EOR         0x8     /* data completes record */ /*notused*/
#define MSG_TRUNC       0x10    /* data discarded before delivery */
#define MSG_CTRUNC      0x20    /* control data lost before delivery */
#define MSG_WAITALL     0x40    /* wait for full request or error */ /*notused*/

#define	MSG_MAXIOVLEN	16

/*
 * Header for ancillary data objects in msg_control buffer.
 * Used for additional information with/about a datagram
 * not expressible by flags.  The format is a sequence
 * of message elements headed by cmsghdr structures.
 * In STREAMware, we shuffle the fields around a little from what
 * they were in net-2, so that they line up the same as an opthdr
 * which simplifies our socket implementation amazingly.
 *
 * Unfortunately, the opthdrs don't include their own length, which the
 * cmsghdrs do.  What this means is that TLI programmers will not be
 * able to take something returned using these macros and immediately give
 * it back to the stack.  The size of the struct cmsghdr will have to be
 * subtracted out.
 * There doesn't seem to be a way to avoid this, since several applications
 * look into the cmsg_len field and won't work if it doesn't include the size
 * of the struct cmsghdr.
 */
struct cmsghdr {
        int     cmsg_level;             /* originating protocol */
        int     cmsg_type;              /* protocol-specific type */
        u_int   cmsg_len;               /* data byte count, including hdr */
/* followed by  u_char  cmsg_data[]; */
};

/* given pointer to struct adatahdr, return pointer to data */
#define CMSG_DATA(cmsg)         ((u_char *)((cmsg) + 1))

/* given pointer to struct adatahdr, return pointer to next adatahdr */
#define CMSG_NXTHDR(mhdr, cmsg) \
        (((caddr_t)(cmsg) + (cmsg)->cmsg_len + sizeof(struct cmsghdr) > \
            (mhdr)->msg_control + (mhdr)->msg_controllen) ? \
            (struct cmsghdr *)NULL : \
            (struct cmsghdr *)((caddr_t)(cmsg) + OPTLEN((cmsg)->cmsg_len)))

#define CMSG_FIRSTHDR(mhdr)     ((struct cmsghdr *)(mhdr)->msg_control)

/* "Socket"-level control message types: */
#define SCM_RIGHTS      0x01            /* access rights (array of int) */

/*
 * This ioctl code uses BSD style ioctls to avoid copyin/out problems.
 * Ioctls have the command encoded in the lower word, and the size of any in
 * or out parameters in the upper word.  The high 2 bits of the upper word
 * are used to encode the in/out status of the parameter; for now we restrict
 * parameters to at most 128 bytes.
 */
#define	IOCPARM_MASK	0x7f	/* parameters must be < 128 bytes */
#define	IOC_VOID	0x20000000	/* no parameters */
#define	IOC_OUT		0x40000000	/* copy out parameters */
#define	IOC_IN		0x80000000	/* copy in parameters */
#define	IOC_INOUT	(IOC_IN|IOC_OUT)
/* the 0x20000000 is so we can distinguish new ioctls from old */
#define	_IOS(x,y)	(IOC_VOID|(x<<8)|y)
#define	_IOSR(x,y,t)	(IOC_OUT|((sizeof(t)&IOCPARM_MASK)<<16)|(x<<8)|y)
#define	_IOSW(x,y,t)	(IOC_IN|((sizeof(t)&IOCPARM_MASK)<<16)|(x<<8)|y)
/* this should be _IOSRW, but stdio got there first */
#define	_IOSWR(x,y,t)	(IOC_INOUT|((sizeof(t)&IOCPARM_MASK)<<16)|(x<<8)|y)

/*
 * Socket ioctl commands
 */

#define SIOCSHIWAT	_IOSW('S', 1, int)	/* set high watermark */
#define SIOCGHIWAT	_IOSR('S', 2, int)	/* get high watermark */
#define SIOCSLOWAT	_IOSW('S', 3, int)	/* set low watermark */
#define SIOCGLOWAT	_IOSR('S', 4, int)	/* get low watermark */
#define SIOCATMARK	_IOSR('S', 5, int)	/* at oob mark? */
#define SIOCSPGRP	_IOSW('S', 6, int)	/* set process group */
#define SIOCGPGRP	_IOSR('S', 7, int)	/* get process group */
#define FIONREAD	_IOSR('S', 8, int)	/* BSD compatibilty */
#define FIONBIO		_IOSW('S', 9, int)	/* BSD compatibilty */
#define FIOASYNC	_IOSW('S', 10, int)	/* BSD compatibilty */
#define SIOCPROTO	_IOSW('S', 11, struct socknewproto)	/* link proto */
#define SIOCGETNAME	_IOSR('S', 12, struct sockaddr)	/* getsockname */
#define SIOCGETPEER	_IOSR('S', 13, struct sockaddr)	/* getpeername */
#define IF_UNITSEL	_IOSW('S', 14, int)	/* set unit number */
#define SIOCXPROTO	_IOS('S', 15)		/* empty proto table */
#define SIOCSHRDTYPE	_IOSW('S', 16, int)	/* set hardware type */

#define	SIOCADDRT	_IOSW('R', 9, struct ortentry)	/* add route */
#define	SIOCDELRT	_IOSW('R', 10, struct ortentry)	/* delete route */

#define	SIOCSIFADDR	_IOSW('I', 11, struct ifreq)	/* set ifnet address */
#define	SIOCGIFADDR	_IOSWR('I', 12, struct ifreq)	/* get ifnet address */
#define	SIOCSIFDSTADDR	_IOSW('I', 13, struct ifreq)	/* set p-p address */
#define	SIOCGIFDSTADDR	_IOSWR('I', 14, struct ifreq)	/* get p-p address */
#define	SIOCSIFFLAGS	_IOSW('I', 15, struct ifreq)	/* set ifnet flags */
#define	SIOCGIFFLAGS	_IOSWR('I', 16, struct ifreq)	/* get ifnet flags */
#define	SIOCGIFCONF	_IOSWR('I', 17, struct ifconf)	/* get ifnet list */

#define	SIOCSIFMTU	_IOSW('I', 21, struct ifreq)	/* get if_mtu */
#define	SIOCGIFMTU	_IOSWR('I', 22, struct ifreq)	/* set if_mtu */


#define	SIOCGIFBRDADDR	_IOSWR('I', 32, struct ifreq)	/* get broadcast addr */
#define	SIOCSIFBRDADDR	_IOSW('I', 33, struct ifreq)	/* set broadcast addr */
#define	SIOCGIFNETMASK	_IOSWR('I', 34, struct ifreq)	/* get net addr mask */
#define	SIOCSIFNETMASK	_IOSW('I', 35, struct ifreq)	/* set net addr mask */
#define	SIOCGIFMETRIC	_IOSWR('I', 36, struct ifreq)	/* get IF metric */
#define	SIOCSIFMETRIC	_IOSW('I', 37, struct ifreq)	/* set IF metric */

#define	SIOCSARP	_IOSW('I', 38, struct arpreq)	/* set arp entry */
#define	SIOCGARP	_IOSWR('I', 39, struct arpreq)	/* get arp entry */
#define	SIOCDARP	_IOSW('I', 40, struct arpreq)	/* delete arp entry */

#define SIOCSIFNAME	_IOSW('I', 41, struct ifreq)	/* set interface name */
#define	SIOCGIFONEP	_IOSWR('I', 42, struct ifreq)	/* get one-packet params */
#define	SIOCSIFONEP	_IOSW('I', 43, struct ifreq)	/* set one-packet params */
#define	SIOCDIFADDR	_IOSW('I', 44, struct ifreq)	/* delete IF addr */
#define	SIOCAIFADDR	_IOSW('I', 45, struct ifaliasreq) /*add/change IF alias*/
#define	SIOCADDMULTI	_IOSW('I', 49, struct ifreq)	/* add m'cast addr */
#define	SIOCDELMULTI	_IOSW('I', 50, struct ifreq)	/* del m'cast addr */
#define	SIOCGIFALIAS	_IOSWR('I', 51, struct ifaliasreq) /* get IF alias */


#define SIOCSOCKSYS	_IOSW('I', 66, struct socksysreq)	/* Pseudo socket syscall */

/* these use ifr_metric to pass the information */
#define SIOCSIFDEBUG	_IOSW('I', 67, struct ifreq)	/* set if debug level */
#define SIOCGIFDEBUG	_IOSWR('I', 68, struct ifreq)	/* get if debug level */

#define SIOCSIFTYPE	_IOSW('I', 69, struct ifreq)	/* set if MIB type */
#define SIOCGIFTYPE	_IOSWR('I', 70, struct ifreq)	/* get if MIB type */

#define SIOCGIFNUM	_IOSR('I', 71, int)		/* get number of ifs */
/*
 * This returns the number of ifreqs that SIOCGIFCONF would return, including
 * aliases.  This is the preferred way of sizing a buffer big enough to hold
 * all interfaces.
 */
#define SIOCGIFANUM	_IOSR('I', 72, int)	/* get number of ifreqs */
/*
 * Interface specific performance tuning
 */
#define	SIOCGIFPERF	_IOSR('I', 73, struct ifreq)	/* get perf info */
#define	SIOCSIFPERF	_IOSR('I', 74, struct ifreq)	/* get perf info */

/*
 * This structure is used to encode pseudo system calls
 */
struct socksysreq {
	/* When porting, make this the widest thing it can be */
	int   args[7];
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
 * utility definitions.
 */

#ifdef MIN
#undef MIN
#endif
#define MIN(x,y) ((x) < (y) ? (x) : (y))
#ifndef MAX
#define MAX(x,y) ((x) > (y) ? (x) : (y))
#endif

#define MAXHOSTNAMELEN	256

#define	NBBY	8		/* number of bits in a byte */


/* defines for user/kernel interface */

#define	MAX_MINOR	(makedev(1,0) - 1)	/* could be non-portable */

#define	SOCKETSYS	140	/* SCO 3.2v5 */

#define  SO_ACCEPT	1
#define  SO_BIND	2
#define  SO_CONNECT	3
#define  SO_GETPEERNAME	4
#define  SO_GETSOCKNAME	5
#define  SO_GETSOCKOPT	6
#define  SO_LISTEN	7
#define  SO_RECV	8
#define  SO_RECVFROM	9
#define  SO_SEND	10
#define  SO_SENDTO	11
#define  SO_SETSOCKOPT	12
#define  SO_SHUTDOWN	13
#define  SO_SOCKET	14
#define  SO_SELECT	15
#define  SO_GETIPDOMAIN	16
#define  SO_SETIPDOMAIN	17
#define  SO_ADJTIME	18
#define  SO_SETREUID	19
#define  SO_SETREGID	20
#define  SO_GETTIME	21
#define  SO_SETTIME	22
#define  SO_GETITIMER	23
#define  SO_SETITIMER	24
#define  SO_RECVMSG	25
#define  SO_SENDMSG	26
#define	 SO_SOCKPAIR	27

/*
 * message flags
 */
#define	M_BCAST	0x80000000

/* Definitions and structures used for extracting */
/* the size and/or the contents of kernel tables */

/* Copyin/out values */
#define	GIARG	0x1
#define	CONTI	0x2
#define	GITAB	0x4

struct gi_arg {
	caddr_t   gi_where;
	unsigned  gi_size;
};

#if !defined(_KERNEL) && !defined(INKERNEL) && !defined(_INKERNEL)

#include <sys/cdefs.h>

__BEGIN_DECLS
int     accept __P_((int, struct sockaddr *, int *));
int     bind __P_((int, const struct sockaddr *, int));
int     connect __P_((int, const struct sockaddr *, int));
int     getpeername __P_((int, struct sockaddr *, int *));
int     getsockname __P_((int, struct sockaddr *, int *));
int     getsockopt __P_((int, int, int, void *, int *));
int     setsockopt __P_((int, int, int, const void *, int));
int     listen __P_((int, int));
ssize_t recv __P_((int, void *, size_t, int));
ssize_t recvfrom __P_((int, void *, size_t, int, struct sockaddr *, int *));
int     recvmsg __P_((int, struct msghdr *, int));
ssize_t send __P_((int, const void *, size_t, int));
int     sendmsg __P_((int, const struct msghdr *, int));
ssize_t sendto __P_((int, const void *, size_t, int, const struct sockaddr *, int));
int     shutdown __P_((int, int));
int     socket __P_((int, int, int));
int     socketpair __P_((int, int, int, int[2]));
__END_DECLS

#endif /* !INKERNEL */
#endif /* __sys_socket_h */
