/*
 * Copyright (c) 1983, 1989, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *      @(#)nameser.h	8.2 (Berkeley) 2/16/94
 * -
 * Portions Copyright (c) 1993 by Digital Equipment Corporation.
 * 
 * Permission to use, copy, modify, and distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies, and that
 * the name of Digital Equipment Corporation not be used in advertising or
 * publicity pertaining to distribution of the document or software without
 * specific, written prior permission.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS" AND DIGITAL EQUIPMENT CORP. DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE, INCLUDING ALL IMPLIED WARRANTIES
 * OF MERCHANTABILITY AND FITNESS.   IN NO EVENT SHALL DIGITAL EQUIPMENT
 * CORPORATION BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS
 * ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS
 * SOFTWARE.
 * -
 * --Copyright--
 */

#ifndef _NAMESER_H_
#define	_NAMESER_H_

#include <endian.h>				
#include <sys/types.h>
#include <pthread/types.h>

/*
 * Define constants based on rfc883
 */
#define PACKETSZ	512		/* maximum packet size */
#define MAXDNAME	256		/* maximum domain name */
#define MAXCDNAME	255		/* maximum compressed domain name */
#define MAXLABEL	63		/* maximum length of domain label */
	/* Number of bytes of fixed size data in query structure */
#define QFIXEDSZ	4
	/* number of bytes of fixed size data in resource record */
#define RRFIXEDSZ	10

/*
 * Internet nameserver port number
 */
#define NAMESERVER_PORT	53

/*
 * Currently defined opcodes
 */
#define QUERY		0x0		/* standard query */
#define IQUERY		0x1		/* inverse query */
#define STATUS		0x2		/* nameserver status query */
/*#define xxx		0x3*/		/* 0x3 reserved */
	/* non standard - supports ALLOW_UPDATES stuff from Mike Schwartz */
#define UPDATEA		0x9		/* add resource record */
#define UPDATED		0xa		/* delete a specific resource record */
#define UPDATEDA	0xb		/* delete all named resource record */
#define UPDATEM		0xc		/* modify a specific resource record */
#define UPDATEMA	0xd		/* modify all named resource record */

#define ZONEINIT	0xe		/* initial zone transfer */
#define ZONEREF		0xf		/* incremental zone referesh */

/*
 * Currently defined response codes
 */
#define NOERROR		0		/* no error */
#define FORMERR		1		/* format error */
#define SERVFAIL	2		/* server failure */
#define NXDOMAIN	3		/* non existent domain */
#define NOTIMP		4		/* not implemented */
#define REFUSED		5		/* query refused */
	/* non standard */
#define NOCHANGE	0xf		/* update failed to change db */

/*
 * Type values for resources and queries
 */
#define T_A		1		/* host address */
#define T_NS		2		/* authoritative server */
#define T_MD		3		/* mail destination */
#define T_MF		4		/* mail forwarder */
#define T_CNAME		5		/* connonical name */
#define T_SOA		6		/* start of authority zone */
#define T_MB		7		/* mailbox domain name */
#define T_MG		8		/* mail group member */
#define T_MR		9		/* mail rename name */
#define T_NULL		10		/* null resource record */
#define T_WKS		11		/* well known service */
#define T_PTR		12		/* domain name pointer */
#define T_HINFO		13		/* host information */
#define T_MINFO		14		/* mailbox information */
#define T_MX		15		/* mail routing information */
#define T_TXT		16		/* text strings */
#define	T_RP		17		/* responsible person */
#define	T_AFSDB		18		/* AFS cell database */
#define	T_NSAP		22		/* NSAP address */
#define	T_NSAP_PTR	23		/* reverse lookup for NSAP */
	/* non standard */
#define T_UINFO		100		/* user (finger) information */
#define T_UID		101		/* user ID */
#define T_GID		102		/* group ID */
#define T_UNSPEC	103		/* Unspecified format (binary data) */
	/* Query type values which do not appear in resource records */
#define T_AXFR		252		/* transfer zone of authority */
#define T_MAILB		253		/* transfer mailbox records */
#define T_MAILA		254		/* transfer mail agent records */
#define T_ANY		255		/* wildcard match */

/*
 * Values for class field
 */

#define C_IN		1		/* the arpa internet */
#define C_CHAOS		3		/* for chaos net (MIT) */
#define C_HS		4		/* for Hesiod name server (MIT) (XXX) */
	/* Query class values which do not appear in resource records */
#define C_ANY		255		/* wildcard match */

/*
 * Status return codes for T_UNSPEC conversion routines
 */
#define CONV_SUCCESS 0
#define CONV_OVERFLOW -1
#define CONV_BADFMT -2
#define CONV_BADCKSUM -3
#define CONV_BADBUFLEN -4

#if !defined(BYTE_ORDER) || (BYTE_ORDER != BIG_ENDIAN \
	&& BYTE_ORDER != LITTLE_ENDIAN && BYTE_ORDER != PDP_ENDIAN)
	/* you must determine what the correct bit order is for
	 * your compiler - the next line is an intentional error
	 * which will force your compiles to bomb until you fix
	 * the above macros.
	 */
  #error "Undefined or invalid BYTE_ORDER";
#endif

/*
 * Structure for query header.  The order of the fields is machine- and
 * compiler-dependent, depending on the byte/bit order and the layout
 * of bit fields.  We use bit fields only in int variables, as this
 * is all ANSI requires.  This requires a somewhat confusing rearrangement.
 */

typedef struct {
	pthread_ipport_type id;		/* query identification number */
#if BYTE_ORDER == BIG_ENDIAN
			/* fields in third byte */
	u_int				qr:1;		/* response flag */
	u_int				opcode:4;	/* purpose of message */
	u_int				aa:1;		/* authoritive answer */
	u_int				tc:1;		/* truncated message */
	u_int				rd:1;		/* recursion desired */
			/* fields in fourth byte */
	u_int				ra:1;		/* recursion available */
	u_int				pr:1;		/* primary server required (non standard) */
	u_int				unused:2;	/* unused bits */
	u_int				rcode:4;	/* response code */
#endif
#if BYTE_ORDER == LITTLE_ENDIAN || BYTE_ORDER == PDP_ENDIAN
			/* fields in third byte */
	u_int				rd:1;		/* recursion desired */
	u_int				tc:1;		/* truncated message */
	u_int				aa:1;		/* authoritive answer */
	u_int				opcode:4;	/* purpose of message */
	u_int				qr:1;		/* response flag */
			/* fields in fourth byte */
	u_int				rcode:4;	/* response code */
	u_int				unused:2;	/* unused bits */
	u_int				pr:1;		/* primary server required (non standard) */
	u_int				ra:1;		/* recursion available */
#endif
			/* remaining bytes */
	pthread_ipport_type qdcount;	/* number of question entries */
	pthread_ipport_type ancount;	/* number of answer entries */
	pthread_ipport_type nscount;	/* number of authority entries */
	pthread_ipport_type arcount;	/* number of resource entries */
} HEADER;

/*
 * Defines for handling compressed domain names
 */
#define INDIR_MASK	0xc0

/*
 * Structure for passing resource records around.
 */
struct rrec {
	pthread_ipport_type	r_zone;		/* zone number */
	pthread_ipport_type	r_class;	/* class number */
	pthread_ipport_type	r_type;		/* type number */
	pthread_ipaddr_type	r_ttl;		/* time to live */
	int					r_size;		/* size of data area */
	char			  * r_data;		/* pointer to data */
};

extern	pthread_ipport_type	_getshort();
extern	pthread_ipaddr_type	_getlong();

/*
 * Inline versions of get/put short/long.  Pointer is advanced.
 * We also assume that a "pthread_ipport_type" holds 2 "chars"
 * and that a "pthread_ipaddr_type" holds 4 "chars".
 *
 * These macros demonstrate the property of C whereby it can be
 * portable or it can be elegant but never both.
 */
#define GETSHORT(s, cp) { \
	register u_char *t_cp = (u_char *)(cp); \
	(s) = ((pthread_ipport_type)t_cp[0] << 8) | (pthread_ipport_type)t_cp[1]; \
	(cp) += 2; \
}

#define GETLONG(l, cp) { \
	register u_char *t_cp = (u_char *)(cp); \
	(l) = (((pthread_ipaddr_type)t_cp[0]) << 24) \
	    | (((pthread_ipaddr_type)t_cp[1]) << 16) \
	    | (((pthread_ipaddr_type)t_cp[2]) << 8) \
	    | (((pthread_ipaddr_type)t_cp[3])); \
	(cp) += 4; \
}

#define PUTSHORT(s, cp) { \
	register pthread_ipport_type t_s = (pthread_ipport_type)(s); \
	register u_char *t_cp = (u_char *)(cp); \
	*t_cp++ = t_s >> 8; \
	*t_cp   = t_s; \
	(cp) += 2; \
}

/*
 * Warning: PUTLONG --no-longer-- destroys its first argument.  if you
 * were depending on this "feature", you will lose.
 */
#define PUTLONG(l, cp) { \
	register pthread_ipaddr_type t_l = (pthread_ipaddr_type)(l); \
	register u_char *t_cp = (u_char *)(cp); \
	*t_cp++ = t_l >> 24; \
	*t_cp++ = t_l >> 16; \
	*t_cp++ = t_l >> 8; \
	*t_cp   = t_l; \
	(cp) += 4; \
}

#endif /* !_NAMESER_H_ */
