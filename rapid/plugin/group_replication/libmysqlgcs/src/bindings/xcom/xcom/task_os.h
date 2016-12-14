/* Copyright (c) 2012, 2016, Oracle and/or its affiliates. All rights reserved.

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

#ifndef TASK_OS_H
#define TASK_OS_H

#ifdef __cplusplus
extern "C" {
#endif

#include "result.h"

#ifdef WIN

#include <winsock2.h>
#include <io.h>
#include <Ws2tcpip.h>
#include <MSWSock.h>

#define DIR_SEP '\\'
#define SOCK_EINTR WSAEINTR
#define SOCK_EAGAIN WSAEINPROGRESS
#define SOCK_EWOULDBLOCK WSAEWOULDBLOCK
#define SOCK_EINPROGRESS WSAEINPROGRESS
#define SOCK_ERRNO task_errno
#define SOCK_OPT_REUSEADDR SO_EXCLUSIVEADDRUSE
#define GET_OS_ERR  WSAGetLastError()
#define SET_OS_ERR(x) WSASetLastError(x)
#define SOCK_ECONNREFUSED WSAECONNREFUSED
#define CLOSESOCKET(x) closesocket(x)

  static inline int hard_connect_err(int err)
  {
	  return err != 0 && from_errno(err) != WSAEINTR && from_errno(err) != WSAEINPROGRESS && from_errno(err) != SOCK_EWOULDBLOCK;
  }

  static inline int hard_select_err(int err)
  {
	  return err != 0 && from_errno(err) != WSAEINTR;
  }


#else
#include <unistd.h>
#include <sys/socket.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>

#define DIR_SEP '/'

/* Solaris and Linux differ here */
#ifndef IPPROTO_TCP
#define IPPROTO_TCP SOL_TCP
#endif

#define SOCK_EINTR EINTR
#define SOCK_EAGAIN EAGAIN
#define SOCK_EWOULDBLOCK EWOULDBLOCK
#define SOCK_EINPROGRESS EINPROGRESS
#define SOCK_ERRNO task_errno
#define SOCK_OPT_REUSEADDR SO_REUSEADDR
#define GET_OS_ERR errno
#define SET_OS_ERR(x) errno = (x)
#define SOCK_ECONNREFUSED ECONNREFUSED
#define CLOSESOCKET(x) close(x)

  static inline int hard_connect_err(int err)
  {
	  return err != 0 && from_errno(err) != EINTR && from_errno(err) != EINPROGRESS;
  }

  static inline int hard_select_err(int err)
  {
	  return from_errno(err) != 0 && from_errno(err) != EINTR;
  }

#endif

	extern void remove_and_wakeup(int fd);

	static inline result close_socket(int *sock)
	{
		result res = {0,0};
		if(*sock != -1){
			do{
				SET_OS_ERR(0);
				res.val = CLOSESOCKET(*sock);
				res.funerr = to_errno(GET_OS_ERR);
			}while(res.val == -1 && from_errno(res.funerr) == SOCK_EINTR);
			remove_and_wakeup(*sock);
			*sock = -1;
		}
		return res;
	}

	static inline result shut_close_socket(int *sock)
	{
		result res = {0,0};
		if(*sock >= 0){
#if defined(WIN32) || defined(WIN64)
			static LPFN_DISCONNECTEX DisconnectEx = NULL;
			if (DisconnectEx == NULL)
			{
				DWORD dwBytesReturned;
				GUID guidDisconnectEx = WSAID_DISCONNECTEX;
				WSAIoctl(*sock, SIO_GET_EXTENSION_FUNCTION_POINTER,
									&guidDisconnectEx, sizeof(GUID),
									&DisconnectEx, sizeof(DisconnectEx),
									&dwBytesReturned, NULL, NULL);
			}
			if (DisconnectEx != NULL)
			{
				(DisconnectEx(*sock, (LPOVERLAPPED) NULL,
											(DWORD) 0, (DWORD) 0) == TRUE) ? 0 : -1;
			}
			else
#endif
			shutdown(*sock, _SHUT_RDWR);
			res = close_socket(sock);
		}
		return res;
	}

#ifdef __cplusplus
}
#endif

#endif

