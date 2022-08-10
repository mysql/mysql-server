/* Copyright (c) 2015, 2022, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is also distributed with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have included with MySQL.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA */

#if defined(linux) && !defined(_XOPEN_SOURCE)
#define _XOPEN_SOURCE
#endif

#if defined(linux) && !defined(_GNU_SOURCE)
#define _GNU_SOURCE
#endif

#include "xcom/xcom_profile.h"
#ifndef _WIN32
#include <netdb.h>
#include <sys/socket.h>
#endif
#include <assert.h>
#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#ifdef _MSC_VER
#include <stdint.h>
#endif

#include "xcom/simset.h"
#include "xcom/task.h"
#include "xcom/task_debug.h"
#include "xcom/task_os.h"
#include "xcom/x_platform.h"
#include "xcom/xcom_memory.h"
#include "xdr_gen/xcom_vp.h"

#define STRING_PORT_SIZE 6
#define NR_GETADDRINFO_ATTEMPTS 10

/**
 * Wrapper function which retries and checks errors from getaddrinfo
 *
 * We have observed that getaddrinfo returns EAI_AGAIN when called with an
 * unresolvable hostname on systems using systemd-resolved for DNS resolution.
 * Therefore, we only attempt to resolve nodename up to NR_GETADDRINFO_ATTEMPTS
 * times, to avoid getting stuck in an infinite loop.
 */
int checked_getaddrinfo(const char *nodename, const char *servname,
                        const struct addrinfo *hints, struct addrinfo **res) {
  int errval = EAI_AGAIN;

  struct addrinfo _hints;
  int attempt_nr;

  memset(&_hints, 0, sizeof(_hints));
  _hints.ai_family = AF_UNSPEC;
  _hints.ai_socktype = SOCK_STREAM; /* TCP stream sockets */
  if (hints == nullptr) hints = &_hints;
  for (attempt_nr = 0;
       errval == EAI_AGAIN && attempt_nr < NR_GETADDRINFO_ATTEMPTS;
       attempt_nr++) {
    if (*res) {
      freeaddrinfo(*res);
      *res = nullptr;
    }
    errval = getaddrinfo(nodename, servname, hints, res);
  }
#if defined(EAI_NODATA) && EAI_NODATA != EAI_NONAME
  /* Solaris may return EAI_NODATA as well as EAI_NONAME */
  if (errval && errval != EAI_NONAME && errval != EAI_NODATA &&
      errval != EAI_AGAIN) {
#else
  /* FreeBSD has removed the definition of EAI_NODATA altogether. */
  if (errval && errval != EAI_NONAME && errval != EAI_AGAIN) {
#endif
#if !defined(_WIN32)
    IFDBG(
        D_NONE, NUMEXP(errval); STREXP(gai_strerror(errval));
        if (errval == EAI_SYSTEM) {
          NUMEXP(errno);
          STREXP(g_strerror(errno));
        });
#else
    IFDBG(D_NONE, NUMEXP(errval); STREXP(gai_strerror(errval)));
#endif
  }
  assert((errval == 0 && *res) || (errval != 0 && *res == nullptr));
  return errval;
}

/**
 @brief Wrapper function to checked_getaddrinfo that accepts a numeric port
 */
int checked_getaddrinfo_port(const char *nodename, xcom_port port,
                             const struct addrinfo *hints,
                             struct addrinfo **res) {
  char buffer[STRING_PORT_SIZE];
  sprintf(buffer, "%d", port);

  return checked_getaddrinfo(nodename, buffer, hints, res);
}

struct infonode;
typedef struct infonode infonode;

struct infonode {
  char *server;
  struct addrinfo *addr;
  infonode *left;
  infonode *right;
};

#ifdef XCOM_STANDALONE
static infonode *addrinfomap;

static infonode *insert_server(infonode **top, char const *server,
                               struct addrinfo *addr) {
  if (!top)
    return 0;
  else {
    if (*top == 0) { /* Insert here */
      infonode *n = (infonode *)xcom_calloc((size_t)1, sizeof(infonode));
      n->server = strdup(server);
      n->addr = addr;
      *top = n;
      return n;
    } else {
      int cmp = strcmp(server, (*top)->server);
      assert(cmp);
      if (cmp == 0) /* Already in map */
        return *top;
      /* Look in subtree */
      if (cmp < 0)
        return insert_server(&(*top)->left, server, addr);
      else
        return insert_server(&(*top)->right, server, addr);
    }
  }
}

static infonode *lookup_server(infonode *top, char const *server) {
  if (!top)
    return 0;
  else {
    int cmp = strcmp(server, top->server);
    if (cmp == 0) return top;
    /* Look in subtree */
    if (cmp < 0)
      return lookup_server(top->left, server);
    else
      return lookup_server(top->right, server);
  }
}

struct addrinfo *xcom_caching_getaddrinfo(char const *server) {
  infonode *n = lookup_server(addrinfomap, server);

  if (n)
    return n->addr;
  else {
    struct addrinfo *addr = 0;
    checked_getaddrinfo(server, 0, 0, &addr);
    if (addr) insert_server(&addrinfomap, server, addr);
    return addr;
  }
}

void free_getaddrinfo_cache(infonode *top) {
  if (top) {
    infonode *right = top->right;
    infonode *left = top->left;
    free(top->server);
    freeaddrinfo(top->addr);
    free(top);
    if (right) free_getaddrinfo_cache(right);
    if (left) free_getaddrinfo_cache(left);
  }
}
#endif

void deinit_network_cache() {
#ifdef XCOM_STANDALONE
  if (addrinfomap) {
    /* purecov: begin deadcode */
    free_getaddrinfo_cache(addrinfomap);
    addrinfomap = NULL;
    /* purecov: end */
  }
#endif
}

#ifdef _WIN32

/* Need to link with Ws2_32.lib */
#pragma comment(lib, "ws2_32.lib")

int init_net() {
  WORD wVersionRequested;
  WSADATA wsaData;
  int err;

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

int deinit_net() {
  WSACleanup();
  deinit_network_cache();
  return 0;
}

#else
int init_net() { return 0; }

int deinit_net() {
  deinit_network_cache();
  return 0;
}
#endif
