/*
 * Copyright (c) 1985, 1990, 1993
 *    The Regents of the University of California.  All rights reserved.
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
 * 	This product includes software developed by the University of
 * 	California, Berkeley and its contributors.
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
 */

#if defined(LIBC_SCCS) && !defined(lint)
static char sccsid[] = "@(#)res_debug.c	8.1 (Berkeley) 6/4/93";
static char rcsid[] = "$Id$";
#endif /* LIBC_SCCS and not lint */

#include <pthread.h>
#include <sys/param.h>
#include <stdio.h>
#include <string.h>
#include <resolv.h>
#include <arpa/inet.h>

void __fp_query();
char *__p_class(), *__p_time(), *__p_type();
char *p_cdname(), *p_fqname(), *p_rr();
static char *p_option __P_((u_long));

char *_res_opcodes[] = {
	"QUERY",
	"IQUERY",
	"CQUERYM",
	"CQUERYU",
	"4",
	"5",
	"6",
	"7",
	"8",
	"UPDATEA",
	"UPDATED",
	"UPDATEDA",
	"UPDATEM",
	"UPDATEMA",
	"ZONEINIT",
	"ZONEREF",
};

char *_res_resultcodes[] = {
	"NOERROR",
	"FORMERR",
	"SERVFAIL",
	"NXDOMAIN",
	"NOTIMP",
	"REFUSED",
	"6",
	"7",
	"8",
	"9",
	"10",
	"11",
	"12",
	"13",
	"14",
	"NOCHANGE",
};

static char retbuf[16];

static char *
dewks(wks)
	int wks;
{
	switch (wks) {
	case 5: return("rje");
	case 7: return("echo");
	case 9: return("discard");
	case 11: return("systat");
	case 13: return("daytime");
	case 15: return("netstat");
	case 17: return("qotd");
	case 19: return("chargen");
	case 20: return("ftp-data");
	case 21: return("ftp");
	case 23: return("telnet");
	case 25: return("smtp");
	case 37: return("time");
	case 39: return("rlp");
	case 42: return("name");
	case 43: return("whois");
	case 53: return("domain");
	case 57: return("apts");
	case 59: return("apfs");
	case 67: return("bootps");
	case 68: return("bootpc");
	case 69: return("tftp");
	case 77: return("rje");
	case 79: return("finger");
	case 87: return("link");
	case 95: return("supdup");
	case 100: return("newacct");
	case 101: return("hostnames");
	case 102: return("iso-tsap");
	case 103: return("x400");
	case 104: return("x400-snd");
	case 105: return("csnet-ns");
	case 109: return("pop-2");
	case 111: return("sunrpc");
	case 113: return("auth");
	case 115: return("sftp");
	case 117: return("uucp-path");
	case 119: return("nntp");
	case 121: return("erpc");
	case 123: return("ntp");
	case 133: return("statsrv");
	case 136: return("profile");
	case 144: return("NeWS");
	case 161: return("snmp");
	case 162: return("snmp-trap");
	case 170: return("print-srv");
	default: (void) sprintf(retbuf, "%d", wks); return(retbuf);
	}
}

static char *
deproto(protonum)
	int protonum;
{
	switch (protonum) {
	case 1: return("icmp");
	case 2: return("igmp");
	case 3: return("ggp");
	case 5: return("st");
	case 6: return("tcp");
	case 7: return("ucl");
	case 8: return("egp");
	case 9: return("igp");
	case 11: return("nvp-II");
	case 12: return("pup");
	case 16: return("chaos");
	case 17: return("udp");
	default: (void) sprintf(retbuf, "%d", protonum); return(retbuf);
	}
}

static char *
do_rrset(msg, cp, cnt, pflag, file, hs)
	int cnt, pflag;
	char *cp,*msg, *hs;
	FILE *file;
{
	int n;
	int sflag;
	/*
	 * Print  answer records
	 */
	sflag = (_res.pfcode & pflag);
	if (n = ntohs(cnt)) {
		if ((!_res.pfcode) || ((sflag) && (_res.pfcode & RES_PRF_HEAD1)))
			fprintf(file, hs);
		while (--n >= 0) {
			cp = p_rr(cp, msg, file);
			if ((cp-msg) > PACKETSZ)
				return (NULL);
		}
		if ((!_res.pfcode) || ((sflag) && (_res.pfcode & RES_PRF_HEAD1)))
			putc('\n', file);
	}
	return(cp);
}

__p_query(msg)
	char *msg;
{
	__fp_query(msg, stdout);
}

/*
 * Print the current options.
 * This is intended to be primarily a debugging routine.
 */
void
__fp_resstat(statp, file)
	struct __res_state *statp;
	FILE *file;
{
	int bit;

	fprintf(file, ";; res options:");
	if (!statp)
		statp = &_res;
	for (bit = 0;  bit < 32;  bit++) {	/* XXX 32 - bad assumption! */
		if (statp->options & (1<<bit))
			fprintf(file, " %s", p_option(1<<bit));
	}
	putc('\n', file);
}

/*
 * Print the contents of a query.
 * This is intended to be primarily a debugging routine.
 */
void
__fp_query(msg,file)
	char *msg;
	FILE *file;
{
	register char *cp;
	register HEADER *hp;
	register int n;

	/*
	 * Print header fields.
	 */
	hp = (HEADER *)msg;
	cp = msg + sizeof(HEADER);
	if ((!_res.pfcode) || (_res.pfcode & RES_PRF_HEADX) || hp->rcode) {
		fprintf(file,";; ->>HEADER<<- opcode: %s, status: %s, id: %d",
			_res_opcodes[hp->opcode],
			_res_resultcodes[hp->rcode],
			ntohs(hp->id));
		putc('\n', file);
	}
	putc(';', file);
	if ((!_res.pfcode) || (_res.pfcode & RES_PRF_HEAD2)) {
		fprintf(file,"; flags:");
		if (hp->qr)
			fprintf(file," qr");
		if (hp->aa)
			fprintf(file," aa");
		if (hp->tc)
			fprintf(file," tc");
		if (hp->rd)
			fprintf(file," rd");
		if (hp->ra)
			fprintf(file," ra");
		if (hp->pr)
			fprintf(file," pr");
	}
	if ((!_res.pfcode) || (_res.pfcode & RES_PRF_HEAD1)) {
		fprintf(file,"; Ques: %d", ntohs(hp->qdcount));
		fprintf(file,", Ans: %d", ntohs(hp->ancount));
		fprintf(file,", Auth: %d", ntohs(hp->nscount));
		fprintf(file,", Addit: %d", ntohs(hp->arcount));
	}
#if 1
	if ((!_res.pfcode) || (_res.pfcode & 
		(RES_PRF_HEADX | RES_PRF_HEAD2 | RES_PRF_HEAD1))) {
		putc('\n',file);
	}
#endif
	/*
	 * Print question records.
	 */
	if (n = ntohs(hp->qdcount)) {
		if ((!_res.pfcode) || (_res.pfcode & RES_PRF_QUES))
			fprintf(file,";; QUESTIONS:\n");
		while (--n >= 0) {
			fprintf(file,";;\t");
			cp = p_cdname(cp, msg, file);
			if (cp == NULL)
				return;
			if ((!_res.pfcode) || (_res.pfcode & RES_PRF_QUES))
				fprintf(file, ", type = %s",
					__p_type(_getshort(cp)));
			cp += sizeof(u_short);
			if ((!_res.pfcode) || (_res.pfcode & RES_PRF_QUES))
				fprintf(file, ", class = %s\n",
					__p_class(_getshort(cp)));
			cp += sizeof(u_short);
			putc('\n', file);
		}
	}
	/*
	 * Print authoritative answer records
	 */
	cp = do_rrset(msg, cp, hp->ancount, RES_PRF_ANS, file,
		      ";; ANSWERS:\n");
	if (cp == NULL)
		return;

	/*
	 * print name server records
	 */
	cp = do_rrset(msg, cp, hp->nscount, RES_PRF_AUTH, file,
		      ";; AUTHORITY RECORDS:\n");
	if (!cp)
		return;

	/*
	 * print additional records
	 */
	cp = do_rrset(msg, cp, hp->arcount, RES_PRF_ADD, file,
		      ";; ADDITIONAL RECORDS:\n");
	if (!cp)
		return;
}

char *
p_cdname(cp, msg, file)
	char *cp, *msg;
	FILE *file;
{
	char name[MAXDNAME];
	int n;

	if ((n = dn_expand((u_char *)msg, (u_char *)cp + MAXCDNAME,
			   (u_char *)cp, (u_char *)name, sizeof(name))) < 0)
		return (NULL);
	if (name[0] == '\0')
		putc('.', file);
	else
		fputs(name, file);
	return (cp + n);
}

char *
p_fqname(cp, msg, file)
	char *cp, *msg;
	FILE *file;
{
	char name[MAXDNAME];
	int n, len;

	if ((n = dn_expand((u_char *)msg, (u_char *)cp + MAXCDNAME,
			   (u_char *)cp, (u_char *)name, sizeof(name))) < 0)
		return (NULL);
	if (name[0] == '\0') {
		putc('.', file);
	} else {
		fputs(name, file);
		if (name[strlen(name) - 1] != '.')
			putc('.', file);
	}
	return (cp + n);
}

/*
 * Print resource record fields in human readable form.
 *
 * Removed calls to non-reentrant routines to simplify varifying
 * POSIX thread-safe implementations. (mevans).
 */
char *
p_rr(cp, msg, file)
	char *cp, *msg;
	FILE *file;
{
	int type, class, dlen, n, c;
	struct in_addr inaddr;
	char *cp1, *cp2;
	u_long tmpttl, t;
	int lcnt;
	char buf[32];

	if ((cp = p_fqname(cp, msg, file)) == NULL)
		return (NULL);			/* compression error */
	type = _getshort(cp);
	cp += sizeof(u_short);
	class = _getshort(cp);
	cp += sizeof(u_short);
	tmpttl = _getlong(cp);
	cp += sizeof(u_long);
	dlen = _getshort(cp);
	cp += sizeof(u_short);
	cp1 = cp;
	if ((!_res.pfcode) || (_res.pfcode & RES_PRF_TTLID))
		fprintf(file, "\t%lu", tmpttl);
	if ((!_res.pfcode) || (_res.pfcode & RES_PRF_CLASS))
		fprintf(file, "\t%s", __p_class(class));
	fprintf(file, "\t%s", __p_type(type));
	/*
	 * Print type specific data, if appropriate
	 */
	switch (type) {
	case T_A:
		switch (class) {
		case C_IN:
		case C_HS:
			bcopy(cp, (char *)&inaddr, sizeof(inaddr));
			if (dlen == 4) {
			  fprintf(file,"\t%s",
				  inet_ntoa_r(inaddr, buf, sizeof(buf)));
			  cp += dlen;
			} else if (dlen == 7) {
				char *address;
				u_char protocol;
				u_short port;

				address = inet_ntoa_r(inaddr,
						      buf, sizeof(buf));
				cp += sizeof(inaddr);
				protocol = *(u_char*)cp;
				cp += sizeof(u_char);
				port = _getshort(cp);
				cp += sizeof(u_short);
				fprintf(file, "\t%s\t; proto %d, port %d",
					address, protocol, port);
			}
			break;
		default:
			cp += dlen;
		}
		break;
	case T_CNAME:
	case T_MB:
	case T_MG:
	case T_MR:
	case T_NS:
	case T_PTR:
		putc('\t', file);
		cp = p_fqname(cp, msg, file);
		break;

	case T_HINFO:
		if (n = *cp++) {
			fprintf(file,"\t%.*s", n, cp);
			cp += n;
		}
		if (n = *cp++) {
			fprintf(file,"\t%.*s", n, cp);
			cp += n;
		}
		break;

	case T_SOA:
		putc('\t', file);
		cp = p_fqname(cp, msg, file);	/* origin */
		putc(' ', file);
		cp = p_fqname(cp, msg, file);	/* mail addr */
		fputs(" (\n", file);
		t = _getlong(cp);  cp += sizeof(u_long);
		fprintf(file,"\t\t\t%lu\t; serial\n", t);
		t = _getlong(cp);  cp += sizeof(u_long);
		fprintf(file,"\t\t\t%lu\t; refresh (%s)\n", t, __p_time(t));
		t = _getlong(cp);  cp += sizeof(u_long);
		fprintf(file,"\t\t\t%lu\t; retry (%s)\n", t, __p_time(t));
		t = _getlong(cp);  cp += sizeof(u_long);
		fprintf(file,"\t\t\t%lu\t; expire (%s)\n", t, __p_time(t));
		t = _getlong(cp);  cp += sizeof(u_long);
		fprintf(file,"\t\t\t%lu )\t; minimum (%s)", t, __p_time(t));
		break;

	case T_MX:
	case T_AFSDB:
		fprintf(file,"\t%d ", _getshort(cp));
		cp += sizeof(u_short);
		cp = p_fqname(cp, msg, file);
		break;

  	case T_TXT:
		(void) fputs("\t\"", file);
		cp2 = cp1 + dlen;
		while (cp < cp2) {
			if (n = (unsigned char) *cp++) {
				for (c = n; c > 0 && cp < cp2; c--)
					if (*cp == '\n') {
					    (void) putc('\\', file);
					    (void) putc(*cp++, file);
					} else
					    (void) putc(*cp++, file);
			}
		}
		putc('"', file);
  		break;

	case T_MINFO:
	case T_RP:
		putc('\t', file);
		cp = p_fqname(cp, msg, file);
		putc(' ', file);
		cp = p_fqname(cp, msg, file);
		break;

	case T_UINFO:
		putc('\t', file);
		fputs(cp, file);
		cp += dlen;
		break;

	case T_UID:
	case T_GID:
		if (dlen == 4) {
			fprintf(file,"\t%u", _getlong(cp));
			cp += sizeof(long);
		}
		break;

	case T_WKS:
		if (dlen < sizeof(u_long) + 1)
			break;
		bcopy(cp, (char *)&inaddr, sizeof(inaddr));
		cp += sizeof(u_long);
		fprintf(file, "\t%s %s ( ",
			inet_ntoa_r(inaddr, buf, sizeof(buf)),
			deproto((int) *cp));
		cp += sizeof(u_char);
		n = 0;
		lcnt = 0;
		while (cp < cp1 + dlen) {
			c = *cp++;
			do {
 				if (c & 0200) {
					if (lcnt == 0) {
						fputs("\n\t\t\t", file);
						lcnt = 5;
					}
					fputs(dewks(n), file);
					putc(' ', file);
					lcnt--;
				}
 				c <<= 1;
			} while (++n & 07);
		}
		putc(')', file);
		break;

#ifdef ALLOW_T_UNSPEC
	case T_UNSPEC:
		{
			int NumBytes = 8;
			char *DataPtr;
			int i;

			if (dlen < NumBytes) NumBytes = dlen;
			fprintf(file, "\tFirst %d bytes of hex data:",
				NumBytes);
			for (i = 0, DataPtr = cp; i < NumBytes; i++, DataPtr++)
				fprintf(file, " %x", *DataPtr);
			cp += dlen;
		}
		break;
#endif /* ALLOW_T_UNSPEC */

	default:
		fprintf(file,"\t?%d?", type);
		cp += dlen;
	}
#if 0
	fprintf(file, "\t; dlen=%d, ttl %s\n", dlen, __p_time(tmpttl));
#else
	putc('\n', file);
#endif
	if (cp - cp1 != dlen) {
		fprintf(file,";; packet size error (found %d, dlen was %d)\n",
			cp - cp1, dlen);
		cp = NULL;
	}
	return (cp);
}

static	char nbuf[40];

/*
 * Return a string for the type
 */
char *
__p_type(type)
	int type;
{
	switch (type) {
	case T_A:
		return("A");
	case T_NS:		/* authoritative server */
		return("NS");
	case T_CNAME:		/* canonical name */
		return("CNAME");
	case T_SOA:		/* start of authority zone */
		return("SOA");
	case T_MB:		/* mailbox domain name */
		return("MB");
	case T_MG:		/* mail group member */
		return("MG");
	case T_MR:		/* mail rename name */
		return("MR");
	case T_NULL:		/* null resource record */
		return("NULL");
	case T_WKS:		/* well known service */
		return("WKS");
	case T_PTR:		/* domain name pointer */
		return("PTR");
	case T_HINFO:		/* host information */
		return("HINFO");
	case T_MINFO:		/* mailbox information */
		return("MINFO");
	case T_MX:		/* mail routing info */
		return("MX");
	case T_TXT:		/* text */
		return("TXT");
	case T_RP:		/* responsible person */
		return("RP");
	case T_AFSDB:		/* AFS cell database */
		return("AFSDB");
	case T_AXFR:		/* zone transfer */
		return("AXFR");
	case T_MAILB:		/* mail box */
		return("MAILB");
	case T_MAILA:		/* mail address */
		return("MAILA");
	case T_ANY:		/* matches any type */
		return("ANY");
	case T_UINFO:
		return("UINFO");
	case T_UID:
		return("UID");
	case T_GID:
		return("GID");
#ifdef ALLOW_T_UNSPEC
	case T_UNSPEC:
		return("UNSPEC");
#endif /* ALLOW_T_UNSPEC */

	default:
		(void)sprintf(nbuf, "%d", type);
		return(nbuf);
	}
}

/*
 * Return a mnemonic for class
 */
char *
__p_class(class)
	int class;
{

	switch (class) {
	case C_IN:		/* internet class */
		return("IN");
	case C_HS:		/* hesiod class */
		return("HS");
	case C_ANY:		/* matches any class */
		return("ANY");
	default:
		(void)sprintf(nbuf, "%d", class);
		return(nbuf);
	}
}

/*
 * Return a mnemonic for an option
 */
static char *
p_option(option)
	u_long option;
{
	switch (option) {
	case RES_INIT:		return "init";
	case RES_DEBUG:		return "debug";
	case RES_AAONLY:	return "aaonly";
	case RES_USEVC:		return "usevc";
	case RES_PRIMARY:	return "primry";
	case RES_IGNTC:		return "igntc";
	case RES_RECURSE:	return "recurs";
	case RES_DEFNAMES:	return "defnam";
	case RES_STAYOPEN:	return "styopn";
	case RES_DNSRCH:	return "dnsrch";
	default:		sprintf(nbuf, "?0x%x?", option); return nbuf;
	}
}

/*
 * Return a mnemonic for a time to live
 */
char *
__p_time(value)
	u_long value;
{
	int secs, mins, hours, days;
	register char *p;

	if (value == 0) {
		strcpy(nbuf, "0 secs");
		return(nbuf);
	}

	secs = value % 60;
	value /= 60;
	mins = value % 60;
	value /= 60;
	hours = value % 24;
	value /= 24;
	days = value;
	value = 0;

#define	PLURALIZE(x)	x, (x == 1) ? "" : "s"
	p = nbuf;
	if (days) {
		(void)sprintf(p, "%d day%s", PLURALIZE(days));
		while (*++p);
	}
	if (hours) {
		if (days)
			*p++ = ' ';
		(void)sprintf(p, "%d hour%s", PLURALIZE(hours));
		while (*++p);
	}
	if (mins) {
		if (days || hours)
			*p++ = ' ';
		(void)sprintf(p, "%d min%s", PLURALIZE(mins));
		while (*++p);
	}
	if (secs || ! (days || hours || mins)) {
		if (days || hours || mins)
			*p++ = ' ';
		(void)sprintf(p, "%d sec%s", PLURALIZE(secs));
	}
	return(nbuf);
}
