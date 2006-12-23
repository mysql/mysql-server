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

bool SocketAuthSimple::client_authenticate(int sockfd)
{
  SocketOutputStream s_output(sockfd);
  SocketInputStream  s_input(sockfd);

  if (m_username)
    s_output.println("%s", m_username);
  else
    s_output.println("");

  if (m_passwd)
    s_output.println("%s", m_passwd);
  else
    s_output.println("");

  char buf[16];
  if (s_input.gets(buf, 16) == 0) return false;
  if (strncmp("ok", buf, 2) == 0)
    return true;

  return false;
}

bool SocketAuthSimple::server_authenticate(int sockfd)
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
