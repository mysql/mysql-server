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
/*static char *sccsid = "from: @(#)res_send.c	6.45 (Berkeley) 2/24/91";*/
static char *rcsid = "$Id$";
#endif /* LIBC_SCCS and not lint */

#include <pthread.h>
#include <stdio.h>
#include <errno.h>
#include <resolv.h>
#include <netdb.h>
#include <time.h>
#include <sys/timers.h>
#include <sys/socket.h>
#include <sys/uio.h>
#include <netinet/in.h>
#include "res_internal.h"

enum { SEND_GIVE_UP = -1, SEND_TRY_NEXT = -2, SEND_TRY_SAME = -3,
		   SEND_TIMEOUT = -4, SEND_TRUNCATED = -5 };

static int send_datagram(int server, int sock, const char *buf, int buflen,
						 char *answer, int anslen, int try,
						 struct res_data *data);
static int send_circuit(int server, const char *buf, int buflen, char *answer,
						int anslen, struct res_data *data);
static int close_save_errno(int sock);

int res_send(const char *buf, int buflen, char *answer, int anslen)
{
	struct res_data *data;
	struct sockaddr_in local;
	int use_virtual_circuit, result, udp_sock, have_seen_same, terrno = 0;
	int try, server;

	data = _res_init();
	if (!data)
		return -1;

	try = 0;
	server = 0;

	/* Try doing connectionless queries if appropriate. */
	if (!(data->state.options & RES_USEVC) && buflen <= PACKETSZ) {
		/* Create and bind a local UDP socket. */
		udp_sock = socket(AF_INET, SOCK_DGRAM, 0);
		if (udp_sock < 0)
			return -1;
		local.sin_family = AF_INET;
		local.sin_addr.s_addr = htonl(INADDR_ANY);
		local.sin_port = htons(0);
		if (bind(udp_sock, (struct sockaddr *) &local, sizeof(local)) < 0) {
			close(udp_sock);
			return -1;
		}

		/* Cycle through the retries and servers, sending off queries and
		 * waiting for responses. */
		for (; try < data->state.retry; try++) {
			for (; server < data->state.nscount; server++) {
				result = send_datagram(server, udp_sock, buf, buflen, answer,
									   anslen, try, data);
				if (result == SEND_TIMEOUT)
					terrno = ETIMEDOUT;
				else if (result != SEND_TRY_NEXT)
					break;
			}
			if (server < data->state.nscount)
				break;
		}

		close(udp_sock);
		if (result < 0)
			errno = (terrno == ETIMEDOUT) ? ETIMEDOUT : ECONNREFUSED;
		else
			errno = 0;
		if (result != SEND_TRUNCATED)
			return (result >= 0) ? result : -1;
	}

	/* Either we have to use the virtual circuit, or the server couldn't
	 * fit its response in a UDP packet.  Cycle through the retries and
	 * servers, sending off queries and waiting for responses.	Allow a
	 * response of SEND_TRY_SAME to cause an extra retry once. */
	for (; try < data->state.retry; try++) {
		for (; server < data->state.nscount; server++) {
			result = send_circuit(server, buf, buflen, answer, anslen, data);
			terrno = errno;
			if (result == SEND_TRY_SAME) {
				if (!have_seen_same)
					server--;
				have_seen_same = 1;
			} else if (result != SEND_TRY_NEXT) {
				break;
			}
		}
	}

	errno = terrno;
	return (result >= 0) ? result : -1;
}

static int send_datagram(int server, int sock, const char *buf, int buflen,
						 char *answer, int anslen, int try,
						 struct res_data *data)
{
	int count, interval;
	struct sockaddr_in local_addr;
	HEADER *request = (HEADER *) buf, *response = (HEADER *) answer;
	struct timespec timeout;
	struct timeval current;
	struct timezone zone;

#ifdef DEBUG_RESOLVER
	if (_res.options & RES_DEBUG) {
	  printf("res_send: request:\n");
	  __p_query(buf);
	}
#endif /* DEBUG_RESOLVER */
	/* Send a packet to the server. */
	count = sendto(sock, buf, buflen, 0,
				   (struct sockaddr *) &data->state.nsaddr_list[server],
				   sizeof(struct sockaddr_in));

	if (count != buflen) {
#ifdef DEBUG_RESOLVER
	    if (count < 0){
		    if (_res.options & RES_DEBUG)
			    perror("send_datagram:sendto");
		}
#endif /* DEBUG_RESOLVER */
		return SEND_TRY_NEXT;
	}

	/* Await a reply with the correct ID. */
	while (1) {
		struct sockaddr_in from;
		int from_len;

		from_len = sizeof(from);
		interval = data->state.retrans << try;
		if (try > 0)
			interval /= data->state.nscount;
		gettimeofday(&current, &zone);
		current.tv_sec += interval;
		TIMEVAL_TO_TIMESPEC(&current, &timeout);
		count = recvfrom_timedwait(sock, answer, anslen, 0,
								   &from, &from_len, &timeout);
		if (count < 0)
			return SEND_TRY_NEXT;
		/* If the ID is wrong, it's from an old query; ignore it. */
		if (response->id == request->id)
			break;
#ifdef DEBUG_RESOLVER
	    if (_res.options & RES_DEBUG) {
		  printf("res_sendto: count=%d, response:\n", count);
		  __p_query(answer);
		}
#endif /* DEBUG_RESOLVER */
	}

	/* Report a truncated response unless RES_IGNTC is set.	 This will
	 * cause the res_send() loop to fall back to TCP. */
	if (response->tc && !(data->state.options & RES_IGNTC))
		return SEND_TRUNCATED;

	return count;
}

static int send_circuit(int server, const char *buf, int buflen, char *answer,
						int anslen, struct res_data *data)
{
	HEADER *response = (HEADER *) answer;
	int sock = -1, result, n, response_len, count;
	unsigned short len;
	struct iovec iov[2];
	char *p, junk[512];

	/* If data->sock is valid, then it's an open connection to the
	 * first server.  Grab it if it's appropriate; close it if not. */
	if (data->sock) {
		if (server == 0)
			sock = data->sock;
		else
			close(data->sock);
		data->sock = -1;
	}

	/* Initialize our socket if we didn't grab it from data. */
	if (sock == -1) {
		sock = socket(AF_INET, SOCK_STREAM, 0);
		if (sock < 0)
			return SEND_GIVE_UP;
		result = connect(sock,
						 (struct sockaddr *) &data->state.nsaddr_list[server],
						 sizeof(struct sockaddr_in));
		if (result < 0) {
			close_save_errno(sock);
			return SEND_TRY_NEXT;
		}
	}

	/* Send length and message. */
	len = htons((unsigned short) buflen);
	iov[0].iov_base = (caddr_t) &len;
	iov[0].iov_len = sizeof(len);
	iov[1].iov_base = (char *) buf;
	iov[1].iov_len = buflen;
	if (writev(sock, iov, 2) != sizeof(len) + buflen) {
		close_save_errno(sock);
		return SEND_TRY_NEXT;
	}

	/* Receive length. */
	p = (char *) &len;
	n = sizeof(len);
	while (n) {
		count = read(sock, p, n);
		if (count <= 0) {
			/* If we got ECONNRESET, the remote server may have restarted,
			 * and we report SEND_TRY_SAME.	 (The main loop will only
			 * allow one of these, so we don't have to worry about looping
			 * indefinitely.) */
			close_save_errno(sock);
			return (errno == ECONNRESET) ? SEND_TRY_SAME : SEND_TRY_NEXT;
		}
		p += count;
		n -= count;
	}
	len = ntohs(len);
	response_len = (len > anslen) ? anslen : len;
	len -= response_len;

	/* Receive message. */
	p = answer;
	n = response_len;
	while (n) {
		count = read(sock, p, n);
		if (count <= 0) {
			close_save_errno(sock);
			return SEND_TRY_NEXT;
		}
		p += count;
		n -= count;
	}

	/* If the reply is longer than our answer buffer, set the truncated
	 * bit and flush the rest of the reply, to keep the connection in
	 * sync. */
	if (len) {
		response->tc = 1;
		while (len) {
			n = (len > sizeof(junk)) ? sizeof(junk) : len;
			count = read(sock, junk, n);
			if (count <= 0) {
				close_save_errno(sock);
				return response_len;
			}
			len -= count;
		}
	}

	/* If this is the first server, and RES_USEVC and RES_STAYOPEN are
	 * both set, save the connection.  Otherwise, close it. */
	if (server == 0 && (data->state.options & RES_USEVC &&
						data->state.options & RES_STAYOPEN))
		data->sock = sock;
	else
		close_save_errno(sock);
	
	return response_len;
}

static int close_save_errno(int sock)
{
	int terrno;

	terrno = errno;
	close(sock);
	errno = terrno;
}
