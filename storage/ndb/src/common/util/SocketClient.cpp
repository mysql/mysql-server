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

SocketClient::SocketClient(const char *server_name, unsigned short port, SocketAuthenticator *sa) :
  m_connect_timeout_millisec(0) // Blocking connect by default
{
  m_auth= sa;
  m_port= port;
  m_server_name= server_name ? strdup(server_name) : 0;
  my_socket_invalidate(&m_sockfd);
}

SocketClient::~SocketClient()
{
  if (m_server_name)
    free(m_server_name);
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

  if (m_server_name)
  {
    memset(&m_servaddr, 0, sizeof(m_servaddr));
    m_servaddr.sin_family = AF_INET;
    m_servaddr.sin_port = htons(m_port);
    // Convert ip address presentation format to numeric format
    if (Ndb_getInAddr(&m_servaddr.sin_addr, m_server_name))
      return false;
  }

  m_sockfd= my_socket_create(AF_INET, SOCK_STREAM, 0);
  if (!my_socket_valid(m_sockfd)) {
    return false;
  }

  DBUG_PRINT("info",("NDB_SOCKET: " MY_SOCKET_FORMAT, MY_SOCKET_FORMAT_VALUE(m_sockfd)));

  return true;
}

int
SocketClient::bind(const char* bindaddress, unsigned short localport)
{
  if (!my_socket_valid(m_sockfd))
    return -1;

  struct sockaddr_in local;
  memset(&local, 0, sizeof(local));
  local.sin_family = AF_INET;
  local.sin_port = htons(localport);
  // Convert ip address presentation format to numeric format
  if (Ndb_getInAddr(&local.sin_addr, bindaddress))
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

  if (my_bind_inet(m_sockfd, &local) == -1)
  {
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
SocketClient::connect(const char *toaddress, unsigned short toport)
{
  if (!my_socket_valid(m_sockfd))
  {
    if (!init())
    {
      return m_sockfd;
    }
  }

  if (toaddress)
  {
    if (m_server_name)
      free(m_server_name);
    m_server_name = strdup(toaddress);
    m_port = toport;
    memset(&m_servaddr, 0, sizeof(m_servaddr));
    m_servaddr.sin_family = AF_INET;
    m_servaddr.sin_port = htons(toport);
    // Convert ip address presentation format to numeric format
    if (Ndb_getInAddr(&m_servaddr.sin_addr, m_server_name))
    {
      my_socket_close(m_sockfd);
      my_socket_invalidate(&m_sockfd);
      return m_sockfd;
    }
  }

  // Set socket non blocking
  if (my_socket_nonblock(m_sockfd, true) < 0)
  {
    my_socket_close(m_sockfd);
    my_socket_invalidate(&m_sockfd);
    return m_sockfd;
  }

  // Start non blocking connect
  int r = my_connect_inet(m_sockfd, &m_servaddr);
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
  if (my_socket_nonblock(m_sockfd, true) < 0)
  {
    my_socket_close(m_sockfd);
    my_socket_invalidate(&m_sockfd);
    return m_sockfd;
  }

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
