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

#ifndef SOCKET_CLIENT_HPP
#define SOCKET_CLIENT_HPP

#include <NdbTCP.h>
class SocketAuthenticator;

class SocketClient
{
  unsigned int m_connect_timeout_millisec;
  unsigned short m_last_used_port;
  SocketAuthenticator *m_auth;
public:
  SocketClient(SocketAuthenticator *sa = 0);
  ~SocketClient();
  bool init();
  void set_connect_timeout(unsigned int timeout_millisec) {
    m_connect_timeout_millisec = timeout_millisec;
  }
  int bind(const char* local_hostname,
           unsigned short local_port);
  NDB_SOCKET_TYPE connect(const char* server_hostname,
                          unsigned short server_port);

  NDB_SOCKET_TYPE m_sockfd;
};

#endif // SOCKET_ClIENT_HPP
