/* ==== net.c ============================================================
 * Copyright (c) 1993, 1995 by Chris Provenzano, proven@athena.mit.edu
 *
 * Copyright (c) 1989 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Tony Nardo of the Johns Hopkins University/Applied Physics Lab.
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
 *  1.00 93/08/26 proven
 *      -Pthread redesign of this file.
 */

#include <pthreadutil.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/param.h>
#include <netinet/in.h>
#include <stdlib.h>
#include <string.h>
#include <netdb.h>
#include <stdio.h>
#include <ctype.h>

#ifndef MAXHOSTNAMELEN
#define MAXHOSTNAMELEN 64
#endif
/*
 * These globals are set initialy and then are only read.
 * They do not need mutexes.
 */
extern int lflag;
char myhostname[MAXHOSTNAMELEN];

/* 
 * These globals change and therefor do need mutexes
 */
pthread_mutex_t spmutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t spcond = PTHREAD_COND_INITIALIZER;
struct servent *sp = NULL;

void netsetup(void)
{
	pthread_mutex_lock(&spmutex); 
	if (sp) {
		fprintf(stderr, "finger: service pointer already initialized.\n");
		exit(2);
	}
	if ((sp = (struct servent *)malloc(sizeof(struct servent) + 4096)) == NULL){
		fprintf(stderr, "finger: Couldn't allocate service pointer.\n");
		exit(2);
	}
	if (getservbyname_r("finger", "tcp", sp, (char *)sp + sizeof(struct servent), 4096) == NULL) {
		fprintf(stderr, "finger: tcp/finger: unknown service\n");
		exit(2);
	}
	if (gethostname(myhostname, MAXHOSTNAMELEN)) {
		fprintf(stderr, "finger: couldn't get my hostname.\n");
		exit(2);
	}
	pthread_cond_broadcast(&spcond);
	pthread_mutex_unlock(&spmutex); 
}

void netsetupwait(void)
{
	pthread_mutex_lock(&spmutex);
	while(sp == NULL) {
		pthread_cond_wait(&spcond, &spmutex);
	}
	pthread_mutex_unlock(&spmutex);
}

void *netfinger(char *name)
{
	pthread_atexit_t atexit_id;
	register int c, lastc;
	struct in_addr defaddr;
	struct hostent *hp;
	struct sockaddr_in sin;
	int s, i, readbuflen;
	char readbuf[1024];
	char *host;

	netsetupwait();
	pthread_atexit_add(&atexit_id, fflush_nrv, NULL);

	if (!(host = strrchr(name, '@'))) {
		host = myhostname;
	} else {
		*host++ = '\0';
	}
	if (!(hp = gethostbyname(host))) {
		if ((defaddr.s_addr = inet_addr(host)) < 0) {
			fprintf(stderr, "[%s] gethostbyname: Unknown host\n", host);
			return;
		}
	}
	sin.sin_family = hp->h_addrtype;
	memcpy((char *)&sin.sin_addr, hp->h_addr, hp->h_length);
	sin.sin_port = sp->s_port;

	if ((s = socket(sin.sin_family, SOCK_STREAM, 0)) < 0) {
		sprintf(readbuf, "[%s]: socket", hp->h_name);
		perror(readbuf);
		return;
	}

	/* have network connection; identify the host connected with */
	if (connect(s, (struct sockaddr *)&sin, sizeof(sin)) < 0) {
		sprintf(readbuf, "[%s]: connect", hp->h_name);
		perror(readbuf);
		close(s);
		return;
	}

	/* -l flag for remote fingerd  */
	if (lflag)
		write(s, "/W ", 3);
	/* send the name followed by <CR><LF> */
	write(s, name, strlen(name));
	write(s, "\r\n", 2);

	/*
	 * Read from the remote system; once we're connected, we assume some
	 * data.  If none arrives, we hang until the user interrupts, or
	 * until the thread timeout expires.
	 *
	 * If we see a <CR> or a <CR> with the high bit set, treat it as
	 * a newline; if followed by a newline character, only output one
	 * newline.
	 *
	 * Otherwise, all high bits are stripped; if it isn't printable and
	 * it isn't a space, we can simply set the 7th bit.  Every ASCII
	 * character with bit 7 set is printable.
	 */ 
	for (readbuflen = read(s, readbuf, 1024), flockfile(stdout), lastc = '\n',
	  printf("[%s]\n", hp->h_name); readbuflen > 0; 
	  readbuflen = read(s, readbuf, 1024)) {
		for (i = 0; i < readbuflen; i++) {
			c = readbuf[i] & 0x7f;
			if (c == 0x0d) {
				c = '\n';
				lastc = '\r';
			} else {
				if (!isprint(c) && !isspace(c))
					c |= 0x40;
				if (lastc != '\r' || c != '\n')
					lastc = c;
				else {
					lastc = '\n';
					continue;
				}
			}
			putchar_unlocked(c);
		}
	}
	if (lastc != '\n')
		putchar_unlocked('\n');
	funlockfile(stdout);
}
