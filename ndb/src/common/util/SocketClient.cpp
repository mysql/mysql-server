/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA */


#include <ndb_global.h>
#include <NdbOut.hpp>

#include <SocketClient.hpp>
#include <SocketAuthenticator.hpp>

SocketClient::SocketClient(const char *server_name, unsigned short port, SocketAuthenticator *sa)
{
  m_auth= sa;
  m_port= port;
  m_server_name= strdup(server_name);
  m_sockfd= -1;
}

SocketClient::~SocketClient()
{
  if (m_server_name)
    free(m_server_name);
  if (m_sockfd >= 0)
    NDB_CLOSE_SOCKET(m_sockfd);
  if (m_auth)
    delete m_auth;
}

bool
SocketClient::init()
{
  if (m_sockfd >= 0)
    NDB_CLOSE_SOCKET(m_sockfd);

  memset(&m_servaddr, 0, sizeof(m_servaddr));
  m_servaddr.sin_family = AF_INET;
  m_servaddr.sin_port = htons(m_port);
  // Convert ip address presentation format to numeric format
  if (Ndb_getInAddr(&m_servaddr.sin_addr, m_server_name))
    return false;

  m_sockfd= socket(AF_INET, SOCK_STREAM, 0);
  if (m_sockfd == NDB_INVALID_SOCKET) {
    return false;
  }
  
  return true;
}

NDB_SOCKET_TYPE
SocketClient::connect()
{
  if (m_sockfd < 0)
  {
    if (!init()) {
      ndbout << "SocketClient::connect() failed " << m_server_name << " " << m_port << endl;
      return -1;
    }
  }
  const int r = ::connect(m_sockfd, (struct sockaddr*) &m_servaddr, sizeof(m_servaddr));
  if (r == -1) {
    NDB_CLOSE_SOCKET(m_sockfd);
    m_sockfd= -1;
    return -1;
  }

  if (m_auth) {
    if (!m_auth->client_authenticate(m_sockfd))
    {
      NDB_CLOSE_SOCKET(m_sockfd);
      m_sockfd= -1;
      return -1;
    }
  }
  NDB_SOCKET_TYPE sockfd= m_sockfd;
  m_sockfd= -1;

  return sockfd;
}
