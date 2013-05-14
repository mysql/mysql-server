/* Copyright (c) 2003-2007 MySQL AB


   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA */



#include <ndb_global.h>

#include <SocketClient.hpp>
#include <SocketAuthenticator.hpp>

SocketClient::SocketClient(const char *server_name, unsigned short port, SocketAuthenticator *sa) :
  m_connect_timeout_millisec(0) // Blocking connect by default
{
  m_auth= sa;
  m_port= port;
  m_server_name= server_name ? strdup(server_name) : 0;
  m_sockfd= NDB_INVALID_SOCKET;
}

SocketClient::~SocketClient()
{
  if (m_server_name)
    free(m_server_name);
  if (m_sockfd != NDB_INVALID_SOCKET)
    NDB_CLOSE_SOCKET(m_sockfd);
  if (m_auth)
    delete m_auth;
}

bool
SocketClient::init()
{
  if (m_sockfd != NDB_INVALID_SOCKET)
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
  
  m_sockfd= socket(AF_INET, SOCK_STREAM, 0);
  if (m_sockfd == NDB_INVALID_SOCKET) {
    return false;
  }

  DBUG_PRINT("info",("NDB_SOCKET: %d", m_sockfd));

  return true;
}

int
SocketClient::bind(const char* bindaddress, unsigned short localport)
{
  if (m_sockfd == NDB_INVALID_SOCKET)
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
  
  const int on = 1;
  if (setsockopt(m_sockfd, SOL_SOCKET, SO_REUSEADDR, 
		 (const char*)&on, sizeof(on)) == -1) {

    int ret = errno;
    NDB_CLOSE_SOCKET(m_sockfd);
    m_sockfd= NDB_INVALID_SOCKET;
    return ret;
  }
  
  if (::bind(m_sockfd, (struct sockaddr*)&local, sizeof(local)) == -1) 
  {
    int ret = errno;
    NDB_CLOSE_SOCKET(m_sockfd);
    m_sockfd= NDB_INVALID_SOCKET;
    return ret;
  }
  
  return 0;
}

NDB_SOCKET_TYPE
SocketClient::connect(const char *toaddress, unsigned short toport)
{
  if (!my_socket_valid(m_sockfd))
  {
    if (!init())
    {
      return NDB_INVALID_SOCKET;
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
      return NDB_INVALID_SOCKET;
  }

  // Set socket non blocking
  const int flags= fcntl(m_sockfd, F_GETFL, 0);
  fcntl(m_sockfd, F_SETFL, flags | O_NONBLOCK);

  // Start non blocking connect
  int r = ::connect(m_sockfd,
		    (struct sockaddr*) &m_servaddr, sizeof(m_servaddr));
  if (r == 0)
    goto done; // connected immediately.

  if (r < 0 && (errno != EINPROGRESS)) {
    // Start of non blocking connect failed
    NDB_CLOSE_SOCKET(m_sockfd);
    m_sockfd= NDB_INVALID_SOCKET;
    return NDB_INVALID_SOCKET;
  }

  if (ndb_poll(m_sockfd, true, true, true,
               m_connect_timeout_millisec > 0 ?
               m_connect_timeout_millisec : -1) <= 0)
  {
    // Nothing has happened on the socket after timeout
    // or an error occured
    NDB_CLOSE_SOCKET(m_sockfd);
    m_sockfd= NDB_INVALID_SOCKET;
    return NDB_INVALID_SOCKET;
  }

  // Activity detected on the socket

  {
    // Check socket level error code
    int so_error = 0;
    SOCKOPT_OPTLEN_TYPE len= sizeof(so_error);
    if (getsockopt(m_sockfd, SOL_SOCKET, SO_ERROR, &so_error, &len) < 0)
    {
      NDB_CLOSE_SOCKET(m_sockfd);
      m_sockfd= NDB_INVALID_SOCKET;
      return NDB_INVALID_SOCKET;
    }

    if (so_error)
    {
      NDB_CLOSE_SOCKET(m_sockfd);
      m_sockfd= NDB_INVALID_SOCKET;
      return NDB_INVALID_SOCKET;
    }
  }

done:
  fcntl(m_sockfd, F_SETFL, flags);

  if (m_auth) {
    if (!m_auth->client_authenticate(m_sockfd))
    {
      NDB_CLOSE_SOCKET(m_sockfd);
      m_sockfd= NDB_INVALID_SOCKET;
      return NDB_INVALID_SOCKET;
    }
  }
  NDB_SOCKET_TYPE sockfd= m_sockfd;
  m_sockfd= NDB_INVALID_SOCKET;

  return sockfd;
}
