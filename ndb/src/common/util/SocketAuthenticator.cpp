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


#include <ndb_global.h>

#include <SocketClient.hpp>
#include <SocketAuthenticator.hpp>
#include <NdbOut.hpp>

SocketAuthSimple::SocketAuthSimple(const char *passwd) {
  m_passwd= strdup(passwd);
  m_buf= (char*)malloc(strlen(passwd)+1);
}

SocketAuthSimple::~SocketAuthSimple()
{
  if (m_passwd)
    free((void*)m_passwd);
  if (m_buf)
    free(m_buf);
}

bool SocketAuthSimple::client_authenticate(int sockfd)
{
  if (!m_passwd)
    return false;

  int len = strlen(m_passwd);
  int r;
  r= send(sockfd, m_passwd, len, 0);

  r= recv(sockfd, m_buf, len, 0);
  m_buf[r]= '\0';

  return true;
}

bool SocketAuthSimple::server_authenticate(int sockfd)
{
  if (!m_passwd)
    return false;

  int len = strlen(m_passwd), r;
  r= recv(sockfd, m_buf, len, 0);
  m_buf[r]= '\0';
  r= send(sockfd, m_passwd, len, 0);

  return true;
}
