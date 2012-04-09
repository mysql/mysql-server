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

#ifndef SOCKET_AUTHENTICATOR_HPP
#define SOCKET_AUTHENTICATOR_HPP

class SocketAuthenticator
{
public:
  SocketAuthenticator() {}
  virtual ~SocketAuthenticator() {};
  virtual bool client_authenticate(NDB_SOCKET_TYPE sockfd) = 0;
  virtual bool server_authenticate(NDB_SOCKET_TYPE sockfd) = 0;
};

class SocketAuthSimple : public SocketAuthenticator
{
  const char *m_passwd;
  const char *m_username;
public:
  SocketAuthSimple(const char *username, const char *passwd);
  virtual ~SocketAuthSimple();
  virtual bool client_authenticate(NDB_SOCKET_TYPE sockfd);
  virtual bool server_authenticate(NDB_SOCKET_TYPE sockfd);
};

#endif // SOCKET_AUTHENTICATOR_HPP
