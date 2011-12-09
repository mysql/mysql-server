/*
   Copyright (c) 2009, 2010, Oracle and/or its affiliates. All rights reserved.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
*/

#include "my_global.h"
#include "m_string.h"
#include "ndb_socket.h"


/*
  Implement my_socketpair() so that it works both on UNIX and windows
*/

#if defined _WIN32

int my_socketpair(ndb_socket_t s[2])
{
  struct sockaddr_in addr;
  SOCKET_SIZE_TYPE addrlen = sizeof(addr);
  ndb_socket_t listener;

  my_socket_invalidate(&listener);
  my_socket_invalidate(&s[0]);
  my_socket_invalidate(&s[1]);

  listener= my_socket_create(AF_INET, SOCK_STREAM, 0);
  if (!my_socket_valid(listener))
    return -1;

  bzero(&addr, sizeof(addr));
  addr.sin_family = AF_INET;
  addr.sin_addr.s_addr = htonl(0x7f000001); /* localhost */
  addr.sin_port = 0; /* Any port */

  /* bind any local address */
  if (my_bind_inet(listener, &addr) == -1)
    goto err;

  /* get sockname */
  /*
    TODO: if there was a my_getsockname, this wouldnt have to use
    definition of my_socket (i.e l.s)
  */
  if (getsockname(listener.s, (struct sockaddr*)&addr, &addrlen) < 0)
    goto err;

  if (my_listen(listener, 1) == -1)
    goto err;

  s[0]= my_socket_create(AF_INET, SOCK_STREAM, 0);

  if (!my_socket_valid(s[0]))
    goto err;

  if (my_connect_inet(s[0], &addr) == -1)
    goto err;

  s[1]= my_accept(listener, 0, 0);
  if (!my_socket_valid(s[1]))
    goto err;

  my_socket_close(listener);
  return 0;

err:
  {
    int save_errno = my_socket_errno();

    if (my_socket_valid(listener))
      my_socket_close(listener);

    if (my_socket_valid(s[0]))
      my_socket_close(s[0]);

    if (my_socket_valid(s[1]))
      my_socket_close(s[1]);

    my_socket_set_errno(save_errno);
  }
  return -1;
}

#else

int my_socketpair(ndb_socket_t s[2])
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

