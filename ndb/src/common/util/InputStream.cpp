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

#include "InputStream.hpp"
#include <socket_io.h>

FileInputStream Stdin(stdin);

FileInputStream::FileInputStream(FILE * file)
  : f(file) {
}

char* 
FileInputStream::gets(char * buf, int bufLen){ 
  if(!feof(f)){
    return fgets(buf, bufLen, f);
  }
  return 0;
}

SocketInputStream::SocketInputStream(NDB_SOCKET_TYPE socket, 
				     unsigned readTimeout) 
  : m_socket(socket) {
  m_startover= true;
  m_timeout = readTimeout;
}

char*
SocketInputStream::gets(char * buf, int bufLen) {
  assert(bufLen >= 2);
  int offset= 0;
  if(m_startover)
  {
    buf[0]= '\0';
    m_startover= false;
  }
  else
    offset= strlen(buf);

  int res = readln_socket(m_socket, m_timeout, buf+offset, bufLen-offset);

  if(res == 0)
  {
    buf[0]=0;
    return buf;
  }

  m_startover= true;

  if(res == -1)
    return 0;

  return buf;
}
