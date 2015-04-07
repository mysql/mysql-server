/*
   Copyright (c) 2004, 2010, Oracle and/or its affiliates. All rights reserved.

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


#include <ndb_global.h>

#include <SocketClient.hpp>
#include <SocketAuthenticator.hpp>

SocketClient::SocketClient(SocketAuthenticator *sa) :
  m_connect_timeout_millisec(0),// Blocking connect by default
  m_last_used_port(0),
  m_auth(sa)
{
  my_socket_invalidate(&m_sockfd);
}

SocketClient::~SocketClient()
{
  if (my_socket_valid(m_sockfd))
    NDB_CLOSE_SOCKET(m_sockfd);
  if (m_auth)
    delete m_auth;
}

bool
SocketClient::init()
{
  if (my_socket_valid(m_sockfd))
    NDB_CLOSE_SOCKET(m_sockfd);

  m_sockfd= my_socket_create(AF_INET, SOCK_STREAM, 0);
  if (!my_socket_valid(m_sockfd)) {
    return false;
  }

  DBUG_PRINT("info",("NDB_SOCKET: " MY_SOCKET_FORMAT, MY_SOCKET_FORMAT_VALUE(m_sockfd)));

  return true;
}

int
SocketClient::bind(const char* local_hostname,
                   unsigned short local_port)
{
  if (!my_socket_valid(m_sockfd))
    return -1;

  struct sockaddr_in local;
  memset(&local, 0, sizeof(local));
  local.sin_family = AF_INET;
  local.sin_port = htons(local_port);
  if (local_port == 0 &&
      m_last_used_port != 0)
  {
    // Try to bind to the same port as last successful connect instead of
    // any ephemeral port. Intention is to reuse any previous TIME_WAIT TCB
    local.sin_port = htons(m_last_used_port);
  }

  // Resolve local address
  if (Ndb_getInAddr(&local.sin_addr, local_hostname))
  {
    return errno ? errno : EINVAL;
  }

  if (my_socket_reuseaddr(m_sockfd, true) == -1)
  {
    int ret = my_socket_errno();
    my_socket_close(m_sockfd);
    my_socket_invalidate(&m_sockfd);
    return ret;
  }

  while (my_bind_inet(m_sockfd, &local) == -1)
  {
    if (local_port == 0 &&
        m_last_used_port != 0)
    {
      // Faild to bind same port as last, retry with any
      // ephemeral port(as originally requested)
      m_last_used_port = 0; // Reset last used port
      local.sin_port = htons(0); // Try bind with any port
      continue;
    }

    int ret = my_socket_errno();
    my_socket_close(m_sockfd);
    my_socket_invalidate(&m_sockfd);
    return ret;
  }

  return 0;
}

#ifdef _WIN32
#define NONBLOCKERR(E) (E!=SOCKET_EAGAIN && E!=SOCKET_EWOULDBLOCK)
#else
#define NONBLOCKERR(E) (E!=EINPROGRESS)
#endif

NDB_SOCKET_TYPE
SocketClient::connect(const char* server_hostname,
                      unsigned short server_port)
{
  // Reset last used port(in case connect fails)
  m_last_used_port = 0;

  if (!my_socket_valid(m_sockfd))
  {
    if (!init())
    {
      return m_sockfd;
    }
  }

  struct sockaddr_in server_addr;
  memset(&server_addr, 0, sizeof(server_addr));
  server_addr.sin_family = AF_INET;
  server_addr.sin_port = htons(server_port);

  // Resolve server address
  if (Ndb_getInAddr(&server_addr.sin_addr, server_hostname))
  {
    my_socket_close(m_sockfd);
    my_socket_invalidate(&m_sockfd);
    return m_sockfd;
  }

  // Set socket non blocking
  if (my_socket_nonblock(m_sockfd, true) < 0)
  {
    my_socket_close(m_sockfd);
    my_socket_invalidate(&m_sockfd);
    return m_sockfd;
  }

  // Start non blocking connect
  int r = my_connect_inet(m_sockfd, &server_addr);
  if (r == 0)
    goto done; // connected immediately.

  if (r < 0 && NONBLOCKERR(my_socket_errno())) {
    // Start of non blocking connect failed
    my_socket_close(m_sockfd);
    my_socket_invalidate(&m_sockfd);
    return m_sockfd;
  }

  if (ndb_poll(m_sockfd, true, true, true,
               m_connect_timeout_millisec > 0 ?
               m_connect_timeout_millisec : -1) <= 0)
  {
    // Nothing has happened on the socket after timeout
    // or an error occured
    my_socket_close(m_sockfd);
    my_socket_invalidate(&m_sockfd);
    return m_sockfd;
  }

  // Activity detected on the socket

  {
    // Check socket level error code
    int so_error = 0;
    SOCKET_SIZE_TYPE len= sizeof(so_error);
    if (my_getsockopt(m_sockfd, SOL_SOCKET, SO_ERROR, &so_error, &len) < 0)
    {
      my_socket_close(m_sockfd);
      my_socket_invalidate(&m_sockfd);
      return m_sockfd;
    }

    if (so_error)
    {
      my_socket_close(m_sockfd);
      my_socket_invalidate(&m_sockfd);
      return m_sockfd;
    }
  }

done:
  if (my_socket_nonblock(m_sockfd, false) < 0)
  {
    my_socket_close(m_sockfd);
    my_socket_invalidate(&m_sockfd);
    return m_sockfd;
  }

  // Remember the local port used for this connection
  assert(m_last_used_port == 0);
  my_socket_get_port(m_sockfd, &m_last_used_port);

  if (m_auth) {
    if (!m_auth->client_authenticate(m_sockfd))
    {
      my_socket_close(m_sockfd);
      my_socket_invalidate(&m_sockfd);
      return m_sockfd;
    }
  }

  NDB_SOCKET_TYPE sockfd = m_sockfd;

  my_socket_invalidate(&m_sockfd);

  return sockfd;
}
