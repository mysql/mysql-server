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

#ifndef SOCKET_CLIENT_HPP
#define SOCKET_CLIENT_HPP

#include <NdbTCP.h>
class SocketAuthenticator;

class SocketClient
{
  NDB_SOCKET_TYPE m_sockfd;
  struct sockaddr_in m_servaddr;
  unsigned short m_port;
  char *m_server_name;
  SocketAuthenticator *m_auth;
public:
  SocketClient(const char *server_name, unsigned short port, SocketAuthenticator *sa = 0);
  ~SocketClient();
  bool init();
  NDB_SOCKET_TYPE connect();
  bool close();
};

#endif // SOCKET_ClIENT_HPP
