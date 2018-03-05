/* Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#if defined(linux) && !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE
#endif

#if defined(linux) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include "x_platform.h"

#include <sys/types.h>

#ifndef WIN
#include <sys/socket.h>
#endif

#include <errno.h>
#include <assert.h>
#include <string.h>
#include <stdlib.h>

#include "simset.h"
#include "task_debug.h"
#include "xcom_vp.h"
#include "task.h"
#include "task_os.h"

/**
 * Wrapper function which retries and checks errors from socket
 */
result	xcom_checked_socket(int domain, int type, int protocol)
{
	result	ret = {0,0};
	int	retry = 1000;
	do {
	  	SET_OS_ERR(0);
		ret.val = socket(domain, type, protocol);
		ret.funerr = to_errno(GET_OS_ERR);
	} while (--retry && ret.val == -1 && (from_errno(ret.funerr) == SOCK_EAGAIN));
	if (ret.val == -1) {
		task_dump_err(ret.funerr);
		/* abort(); */
	}
	return ret;
}


/**
 * Wrapper function which retries and checks errors from getaddrinfo
 */
int
checked_getaddrinfo(const char *nodename, const char *servname,
const struct addrinfo *hints, struct addrinfo **res)
{
	int	errval = 0;
	/** FIXME: Lookup IPv4 only for now */
	struct addrinfo _hints;
	memset(&_hints, 0, sizeof(_hints));
	_hints.ai_family = PF_INET;
	if (hints == NULL)
		hints = &_hints;
	do {
		if (*res) {
			freeaddrinfo(*res);
			*res = NULL;
		}
		errval = getaddrinfo(nodename, servname, hints, res);
	} while (errval == EAI_AGAIN);
#if defined(EAI_NODATA) && EAI_NODATA != EAI_NONAME
        /* Solaris may return EAI_NODATA as well as EAI_NONAME */
        if (errval && errval != EAI_NONAME && errval != EAI_NODATA) {
#else
        /* FreeBSD has removed the definition of EAI_NODATA altogether. */
        if (errval && errval != EAI_NONAME) {
#endif
#if !defined(WIN32) && !defined(WIN64)
		DBGOUT(NUMEXP(errval); STREXP(gai_strerror(errval));
		    if (errval == EAI_SYSTEM) {
			NUMEXP(errno); STREXP(g_strerror(errno));
		}
		);
#else
		DBGOUT(NUMEXP(errval); STREXP(gai_strerror(errval)));
#endif
	}
	assert((errval == 0 && *res) || (errval != 0 && *res == NULL));
	return errval;
}

struct infonode;
typedef struct infonode infonode;

struct infonode {
	char const *server;
	struct addrinfo *addr;
	infonode *left;
	infonode *right;
};

static infonode *addrinfomap;

static infonode *insert_server(infonode **top, char const *server, struct addrinfo *addr)
{
	if (!top)
		return 0;
	else {
		if (*top == 0) { /* Insert here */
			infonode * n = calloc(1, sizeof(infonode));
			n->server = strdup(server);
			n->addr = addr;
			*top = n;
			return n;
		} else {
			int	cmp = strcmp(server, (*top)->server);
			assert(cmp);
			if (cmp == 0) /* Already in map */
				return * top;
			/* Look in subtree */
			if (cmp < 0)
				return insert_server(&(*top)->left, server, addr);
			else
				return insert_server(&(*top)->right, server, addr);
		}
	}
}

static infonode *lookup_server(infonode *top, char const *server)
{
	if (!top)
		return 0;
	else {
		int	cmp = strcmp(server, top->server);
		if (cmp == 0)
			return top;
		/* Look in subtree */
		if (cmp < 0)
			return lookup_server(top->left, server);
		else
			return lookup_server(top->right, server);
	}
}


struct addrinfo *caching_getaddrinfo(char const *server)
{
	infonode * n = lookup_server(addrinfomap, server);

	if (n)
		return n->addr;
	else {
		struct addrinfo *addr = 0;
		checked_getaddrinfo(server, 0, 0, &addr);
		if (addr)
			insert_server(&addrinfomap, server, addr);
		return addr;
	}
}

void free_getaddrinfo_cache(infonode *top)
{
  if (top)
  {
    infonode* right= top->right;
    infonode* left= top->left;
    free((char *)top->server);
    freeaddrinfo(top->addr);
    free(top);
    if (right)
      free_getaddrinfo_cache(right);
    if (left)
      free_getaddrinfo_cache(left);
  }
}

#ifdef WIN

/* Need to link with Ws2_32.lib */
#pragma comment(lib, "ws2_32.lib")


int init_net()
{

	WORD wVersionRequested;
	WSADATA wsaData;
	int	err;

	/* Use the MAKEWORD(lowbyte, highbyte) macro declared in Windef.h */
	wVersionRequested = MAKEWORD(2, 2);

	err = WSAStartup(wVersionRequested, &wsaData);
	if (err != 0) {
		/* Tell the user that we could not find a usable */
		/* Winsock DLL. */
		g_critical("WSAStartup failed with error: %d", err);
		return 1;
	}

	/* Confirm that the WinSock DLL supports 2.2.*/
	/* Note that if the DLL supports versions greater    */
	/* than 2.2 in addition to 2.2, it will still return */
	/* 2.2 in wVersion since that is the version we      */
	/* requested.                                        */

	if (LOBYTE(wsaData.wVersion) != 2 || HIBYTE(wsaData.wVersion) != 2) {
		/* Tell the user that we could not find a usable */
		/* WinSock DLL.                                  */
		g_critical("Could not find a usable version of Winsock.dll");
		return 1;
	} else {
		G_DEBUG("The Winsock 2.2 dll was found okay");
		return 0;
	}
}

int deinit_net()
{
    WSACleanup();
    if (addrinfomap)
    {
      free_getaddrinfo_cache(addrinfomap);
      addrinfomap= NULL;
    }
    return 0;
}

#else
int init_net()
{
  return 0;
}

int deinit_net()
{
  if (addrinfomap)
  {
    free_getaddrinfo_cache(addrinfomap);
    addrinfomap= NULL;
  }
  return 0;
}

#endif
