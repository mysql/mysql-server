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

#ifndef SOCKET_CLIENT_HPP
#define SOCKET_CLIENT_HPP

#include <NdbTCP.h>
class SocketAuthenticator;

class SocketClient
{
  NDB_SOCKET_TYPE m_sockfd;
  struct sockaddr_in m_servaddr;
  unsigned int m_connect_timeout_sec;
  unsigned short m_port;
  char *m_server_name;
  SocketAuthenticator *m_auth;
public:
  SocketClient(const char *server_name, unsigned short port, SocketAuthenticator *sa = 0);
  ~SocketClient();
  bool init();
  void set_port(unsigned short port) {
    m_port = port;
    m_servaddr.sin_port = htons(m_port);
  };
  void set_connect_timeout(unsigned int s) {
    m_connect_timeout_sec= s;
  }
  unsigned short get_port() { return m_port; };
  char *get_server_name() { return m_server_name; };
  int bind(const char* toaddress, unsigned short toport);
  NDB_SOCKET_TYPE connect(const char* toaddress = 0, unsigned short port = 0);
  bool close();
};

#endif // SOCKET_ClIENT_HPP
