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
  m_timeout = readTimeout; 
}

char* 
SocketInputStream::gets(char * buf, int bufLen) {
  buf[0] = 77;
  assert(bufLen >= 2);
  int res = readln_socket(m_socket, m_timeout, buf, bufLen - 1);
  if(res == -1)
    return 0;
  if(res == 0 && buf[0] == 77){ // select return 0
    buf[0] = 0;
  } else if(res == 0 && buf[0] == 0){ // only newline
    buf[0] = '\n';
    buf[1] = 0;
  } else {
    int len = strlen(buf);
    buf[len + 1] = '\0';
    buf[len] = '\n';
  }
  return buf;
}
