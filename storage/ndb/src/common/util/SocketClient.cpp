/* Copyright (C) 2003 MySQL AB

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; version 2 of the License.

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
  m_server_name= server_name ? strdup(server_name) : 0;
  m_sockfd= NDB_INVALID_SOCKET;
  m_connect_timeout_sec= 0;
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
  fd_set rset, wset;
  struct timeval tval;
  int r;
  bool use_timeout;
  SOCKOPT_OPTLEN_TYPE len;
  int flags;

  if (m_sockfd == NDB_INVALID_SOCKET)
  {
    if (!init()) {
#ifdef VM_TRACE
      ndbout << "SocketClient::connect() failed " << m_server_name << " " << m_port << endl;
#endif
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

  flags= fcntl(m_sockfd, F_GETFL, 0);
  fcntl(m_sockfd, F_SETFL, flags | O_NONBLOCK);

  r= ::connect(m_sockfd, (struct sockaddr*) &m_servaddr, sizeof(m_servaddr));

  if (r == 0)
    goto done; // connected immediately.

  if (r < 0 && (errno != EINPROGRESS)) {
    NDB_CLOSE_SOCKET(m_sockfd);
    m_sockfd= NDB_INVALID_SOCKET;
    return NDB_INVALID_SOCKET;
  }

  FD_ZERO(&rset);
  FD_SET(m_sockfd, &rset);
  wset= rset;
  tval.tv_sec= m_connect_timeout_sec;
  tval.tv_usec= 0;
  use_timeout= m_connect_timeout_sec;

  if ((r= select(m_sockfd+1, &rset, &wset, NULL,
                 use_timeout? &tval : NULL)) == 0)
  {
    NDB_CLOSE_SOCKET(m_sockfd);
    m_sockfd= NDB_INVALID_SOCKET;
    return NDB_INVALID_SOCKET;
  }

  if (FD_ISSET(m_sockfd, &rset) || FD_ISSET(m_sockfd, &wset))
  {
    len= sizeof(r);
    if (getsockopt(m_sockfd, SOL_SOCKET, SO_ERROR, &r, &len) < 0 || r)
    {
      // Solaris got an error... different than others
      NDB_CLOSE_SOCKET(m_sockfd);
      m_sockfd= NDB_INVALID_SOCKET;
      return NDB_INVALID_SOCKET;
    }
  }
  else
  {
    // select error, probably m_sockfd not set.
    NDB_CLOSE_SOCKET(m_sockfd);
    m_sockfd= NDB_INVALID_SOCKET;
    return NDB_INVALID_SOCKET;
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
