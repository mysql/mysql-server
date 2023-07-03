/*
   Copyright (c) 2004, 2023, Oracle and/or its affiliates.

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

#include <ndb_global.h>
#include <SocketAuthenticator.hpp>
#include <InputStream.hpp>
#include <OutputStream.hpp>
SocketAuthSimple::SocketAuthSimple(const char *username, const char *passwd) {
  if (username)
    m_username= strdup(username);
  else
    m_username= nullptr;
  if (passwd)
    m_passwd= strdup(passwd);
  else
    m_passwd= nullptr;
}

SocketAuthSimple::~SocketAuthSimple()
{
  if (m_passwd)
    free(m_passwd);
  if (m_username)
    free(m_username);
}

bool SocketAuthSimple::client_authenticate(NdbSocket & sockfd)
{
  SecureSocketOutputStream s_output(sockfd);
  SecureSocketInputStream  s_input(sockfd);

  // Write username and password
  s_output.println("%s", m_username ? m_username : "");
  s_output.println("%s", m_passwd ? m_passwd : "");

  char buf[16];

  // Read authentication result
  if (s_input.gets(buf, sizeof(buf)) == nullptr)
    return false;
  buf[sizeof(buf)-1]= 0;

  // Verify authentication result
  if (strncmp("ok", buf, 2) == 0)
    return true;

  return false;
}

bool SocketAuthSimple::server_authenticate(NdbSocket & sockfd)
{
  SecureSocketOutputStream s_output(sockfd);
  SecureSocketInputStream  s_input(sockfd);

  char buf[256];

  // Read username
  if (s_input.gets(buf, sizeof(buf)) == nullptr)
    return false;
  buf[sizeof(buf)-1]= 0;

  // Read password
  if (s_input.gets(buf, sizeof(buf)) == nullptr)
    return false;
  buf[sizeof(buf)-1]= 0;

  // Write authentication result
  s_output.println("ok");

  return true;
}

