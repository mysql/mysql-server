/*
   Copyright (c) 2009, 2024, Oracle and/or its affiliates.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License, version 2.0,
   as published by the Free Software Foundation.

   This program is designed to work with certain software (including
   but not limited to OpenSSL) that is licensed under separate terms,
   as designated in a particular file or component or in included license
   documentation.  The authors of MySQL hereby grant you an additional
   permission to link the program and your derivative works with the
   separately licensed software that they have either included with
   the program or referenced in the documentation.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License, version 2.0, for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include <cstring>
#include "ndb_sockaddr.h"
#include "ndb_socket.h"
#ifdef _WIN32
#include <Winsock2.h>
#include <Ws2ipdef.h>
#else
#include <netinet/in.h>
#endif

/*
  Implement ndb_socketpair() so that it works both on UNIX and windows
*/

#if defined _WIN32

int ndb_socketpair(ndb_socket_t s[2]) {
  ndb_sockaddr addr;
  if (addr.get_address_family() == AF_INET6) {
    addr = ndb_sockaddr(&in6addr_loopback, 0);
  } else {
    in_addr in4;
    in4.s_addr = htonl(INADDR_LOOPBACK);
    addr = ndb_sockaddr(&in4, 0);
  }

  ndb_socket_t listener = ndb_socket_create(addr.get_address_family());
  if (!ndb_socket_valid(listener)) return -1;

  /* bind any local address */
  if (ndb_bind(listener, &addr) == -1) goto err;

  /* get sockname */
  if (ndb_getsockname(listener, &addr) != 0) goto err;

  if (ndb_listen(listener, 1) == -1) goto err;

  s[0] = ndb_socket_create(addr.get_address_family());
  if (!ndb_socket_valid(s[0])) goto err;

  if (ndb_connect(s[0], &addr) == -1) goto err;

  s[1] = ndb_accept(listener, nullptr);
  if (!ndb_socket_valid(s[1])) goto err;

  ndb_socket_close(listener);
  return 0;

err : {
  const int save_errno = WSAGetLastError();

  if (ndb_socket_valid(listener)) ndb_socket_close(listener);

  if (ndb_socket_valid(s[0])) ndb_socket_close(s[0]);

  if (ndb_socket_valid(s[1])) ndb_socket_close(s[1]);

  WSASetLastError(save_errno);
}
  return -1;
}

#else

int ndb_socketpair(ndb_socket_t s[2]) {
  int ret;
  int sock[2];
  ret = socketpair(AF_UNIX, SOCK_STREAM, 0, sock);
  if (ret == 0) {
    s[0] = ndb_socket_create_from_native(sock[0]);
    s[1] = ndb_socket_create_from_native(sock[1]);
  }
  return ret;
}

#endif
