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

#ifndef SOCKET_AUTHENTICATOR_HPP
#define SOCKET_AUTHENTICATOR_HPP

class SocketAuthenticator
{
public:
  virtual ~SocketAuthenticator() {};
  virtual bool client_authenticate(int sockfd) = 0;
  virtual bool server_authenticate(int sockfd) = 0;
};

class SocketAuthSimple : public SocketAuthenticator
{
  const char *m_passwd;
  const char *m_username;
public:
  SocketAuthSimple(const char *username, const char *passwd);
  virtual ~SocketAuthSimple();
  virtual bool client_authenticate(int sockfd);
  virtual bool server_authenticate(int sockfd);
};

#endif // SOCKET_AUTHENTICATOR_HPP
