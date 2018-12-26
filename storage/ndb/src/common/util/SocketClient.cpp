/*
   Copyright (c) 2004, 2017, Oracle and/or its affiliates. All rights reserved.

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


#include <ndb_global.h>

#include <SocketClient.hpp>
#include <SocketAuthenticator.hpp>

SocketClient::SocketClient(SocketAuthenticator *sa) :
  m_connect_timeout_millisec(0),// Blocking connect by default
  m_last_used_port(0),
  m_auth(sa)
{
  ndb_socket_invalidate(&m_sockfd);
}

SocketClient::~SocketClient()
{
  if (ndb_socket_valid(m_sockfd))
    ndb_socket_close(m_sockfd);
  if (m_auth)
    delete m_auth;
}

bool
SocketClient::init()
{
  if (ndb_socket_valid(m_sockfd))
    ndb_socket_close(m_sockfd);

  m_sockfd= ndb_socket_create(AF_INET, SOCK_STREAM, 0);
  if (!ndb_socket_valid(m_sockfd)) {
    return false;
  }

  DBUG_PRINT("info",("NDB_SOCKET: " MY_SOCKET_FORMAT,
                     MY_SOCKET_FORMAT_VALUE(m_sockfd)));

  return true;
}

int
SocketClient::bind(const char* local_hostname,
                   unsigned short local_port)
{
  if (!ndb_socket_valid(m_sockfd))
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

  if (ndb_socket_reuseaddr(m_sockfd, true) == -1)
  {
    int ret = ndb_socket_errno();
    ndb_socket_close(m_sockfd);
    ndb_socket_invalidate(&m_sockfd);
    return ret;
  }

  while (ndb_bind_inet(m_sockfd, &local) == -1)
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

    int ret = ndb_socket_errno();
    ndb_socket_close(m_sockfd);
    ndb_socket_invalidate(&m_sockfd);
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

  if (!ndb_socket_valid(m_sockfd))
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
    ndb_socket_close(m_sockfd);
    ndb_socket_invalidate(&m_sockfd);
    return m_sockfd;
  }

  // Set socket non blocking
  if (ndb_socket_nonblock(m_sockfd, true) < 0)
  {
    ndb_socket_close(m_sockfd);
    ndb_socket_invalidate(&m_sockfd);
    return m_sockfd;
  }

  // Start non blocking connect
  int r = ndb_connect_inet(m_sockfd, &server_addr);
  if (r == 0)
    goto done; // connected immediately.

  if (r < 0 && NONBLOCKERR(ndb_socket_errno())) {
    // Start of non blocking connect failed
    ndb_socket_close(m_sockfd);
    ndb_socket_invalidate(&m_sockfd);
    return m_sockfd;
  }

  if (ndb_poll(m_sockfd, true, true, true,
               m_connect_timeout_millisec > 0 ?
               m_connect_timeout_millisec : -1) <= 0)
  {
    // Nothing has happened on the socket after timeout
    // or an error occured
    ndb_socket_close(m_sockfd);
    ndb_socket_invalidate(&m_sockfd);
    return m_sockfd;
  }

  // Activity detected on the socket

  {
    // Check socket level error code
    int so_error = 0;
    ndb_socket_len_t len= sizeof(so_error);
    if (ndb_getsockopt(m_sockfd, SOL_SOCKET, SO_ERROR, &so_error, &len) < 0)
    {
      ndb_socket_close(m_sockfd);
      ndb_socket_invalidate(&m_sockfd);
      return m_sockfd;
    }

    if (so_error)
    {
      ndb_socket_close(m_sockfd);
      ndb_socket_invalidate(&m_sockfd);
      return m_sockfd;
    }
  }

done:
  if (ndb_socket_nonblock(m_sockfd, false) < 0)
  {
    ndb_socket_close(m_sockfd);
    ndb_socket_invalidate(&m_sockfd);
    return m_sockfd;
  }

  // Remember the local port used for this connection
  assert(m_last_used_port == 0);
  ndb_socket_get_port(m_sockfd, &m_last_used_port);

  if (m_auth) {
    if (!m_auth->client_authenticate(m_sockfd))
    {
      ndb_socket_close(m_sockfd);
      ndb_socket_invalidate(&m_sockfd);
      return m_sockfd;
    }
  }

  NDB_SOCKET_TYPE sockfd = m_sockfd;

  ndb_socket_invalidate(&m_sockfd);

  return sockfd;
}
