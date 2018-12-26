/*
   Copyright (c) 2009, 2017, Oracle and/or its affiliates. All rights reserved.

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
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/


#include "ndb_socket.h"

/*
  Implement ndb_socketpair() so that it works both on UNIX and windows
*/

#if defined _WIN32

int ndb_socketpair(ndb_socket_t s[2])
{
  struct sockaddr_in addr;
  ndb_socket_len_t addrlen = sizeof(addr);
  ndb_socket_t listener;

  ndb_socket_invalidate(&listener);
  ndb_socket_invalidate(&s[0]);
  ndb_socket_invalidate(&s[1]);

  listener= ndb_socket_create(AF_INET, SOCK_STREAM, 0);
  if (!ndb_socket_valid(listener))
    return -1;

  bzero(&addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(0x7f000001); /* localhost */
  addr.sin_port = 0; /* Any port */

  /* bind any local address */
  if (ndb_bind_inet(listener, &addr) == -1)
    goto err;

  /* get sockname */
  if (ndb_getsockname(listener, (struct sockaddr*)&addr, &addrlen) != 0)
    goto err;

  if (ndb_listen(listener, 1) == -1)
    goto err;

  s[0]= ndb_socket_create(AF_INET, SOCK_STREAM, 0);

  if (!ndb_socket_valid(s[0]))
    goto err;

  if (ndb_connect_inet(s[0], &addr) == -1)
    goto err;

  s[1]= ndb_accept(listener, 0, 0);
  if (!ndb_socket_valid(s[1]))
    goto err;

  ndb_socket_close(listener);
  return 0;

err:
  {
    const int save_errno = WSAGetLastError();

    if (ndb_socket_valid(listener))
      ndb_socket_close(listener);

    if (ndb_socket_valid(s[0]))
      ndb_socket_close(s[0]);

    if (ndb_socket_valid(s[1]))
      ndb_socket_close(s[1]);

    WSASetLastError(save_errno);
  }
  return -1;
}

#else

int ndb_socketpair(ndb_socket_t s[2])
{
  int ret;
  int sock[2];
  ret= socketpair(AF_UNIX, SOCK_STREAM, 0, sock);
  if (ret == 0)
  {
    s[0].fd = sock[0];
    s[1].fd = sock[1];
  }
  return ret;
}

#endif

