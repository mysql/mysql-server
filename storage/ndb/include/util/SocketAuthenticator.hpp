/*
   Copyright (c) 2004, 2022, Oracle and/or its affiliates.

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

#ifndef SOCKET_AUTHENTICATOR_HPP
#define SOCKET_AUTHENTICATOR_HPP

#include "portlib/ndb_socket.h"

class SocketAuthenticator
{
public:
  SocketAuthenticator() {}
  virtual ~SocketAuthenticator() {}
  virtual bool client_authenticate(ndb_socket_t sockfd) = 0;
  virtual bool server_authenticate(ndb_socket_t sockfd) = 0;
};

class SocketAuthSimple : public SocketAuthenticator
{
  char *m_passwd;
  char *m_username;
public:
  SocketAuthSimple(const char *username, const char *passwd);
  ~SocketAuthSimple() override;
  bool client_authenticate(ndb_socket_t sockfd) override;
  bool server_authenticate(ndb_socket_t sockfd) override;
};

#endif // SOCKET_AUTHENTICATOR_HPP
