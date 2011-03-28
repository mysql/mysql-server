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
#include <InputStream.hpp>
#include <OutputStream.hpp>
#include <NdbOut.hpp>

SocketAuthSimple::SocketAuthSimple(const char *username, const char *passwd) {
  if (username)
    m_username= strdup(username);
  else
    m_username= 0;
  if (passwd)
    m_passwd= strdup(passwd);
  else
    m_passwd= 0;
}

SocketAuthSimple::~SocketAuthSimple()
{
  if (m_passwd)
    free((void*)m_passwd);
  if (m_username)
    free((void*)m_username);
}

bool SocketAuthSimple::client_authenticate(NDB_SOCKET_TYPE sockfd)
{
  SocketOutputStream s_output(sockfd);
  SocketInputStream  s_input(sockfd);

  s_output.println("%s", m_username ? m_username : "");
  s_output.println("%s", m_passwd ? m_passwd : "");

  char buf[16];
  if (s_input.gets(buf, 16) == 0) return false;
  if (strncmp("ok", buf, 2) == 0)
    return true;

  return false;
}

bool SocketAuthSimple::server_authenticate(NDB_SOCKET_TYPE sockfd)
{

  SocketOutputStream s_output(sockfd);
  SocketInputStream  s_input(sockfd);

  char buf[256];

  if (s_input.gets(buf, 256) == 0) return false;
  buf[255]= 0;
  if (m_username)
    free((void*)m_username);
  m_username= strdup(buf);

  if (s_input.gets(buf, 256) == 0) return false;
  buf[255]= 0;
  if (m_passwd)
    free((void*)m_passwd);
  m_passwd= strdup(buf);

  s_output.println("ok");

  return true;
}
