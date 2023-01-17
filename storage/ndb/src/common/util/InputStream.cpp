/*
   Copyright (c) 2003, 2023, Oracle and/or its affiliates.
    Use is subject to license terms.

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


#include "ndb_global.h"

#include "InputStream.hpp"

FileInputStream Stdin(stdin);

FileInputStream::FileInputStream(FILE * file)
  : f(file) {
}

char* 
FileInputStream::gets(char * buf, int bufLen){ 
  if(!feof(f)){
    return fgets(buf, bufLen, f);
  }
  return nullptr;
}

SecureSocketInputStream::SecureSocketInputStream(const NdbSocket & socket,
                                                 unsigned read_timeout_ms)
  : m_socket(socket) {
  m_startover= true;
  m_timeout_remain= m_timeout_ms = read_timeout_ms;

  m_timedout= false;
}

char*
SecureSocketInputStream::gets(char * buf, int bufLen) {
  if(timedout())
    return nullptr;
  assert(bufLen >= 2);
  int offset= 0;
  if(m_startover)
  {
    buf[0]= '\0';
    m_startover= false;
  }
  else
    offset= (int)strlen(buf);

  int time= 0;
  int res = m_socket.readln(m_timeout_remain, &time,
                            buf+offset, bufLen-offset, m_mutex);

  if(res >= 0)
    m_timeout_remain-=time;
  if(res == 0 || m_timeout_remain<=0)
  {
    m_timedout= true;
    buf[0]=0;
    return buf;
  }

  m_startover= true;

  if(res == -1)
  {
    return nullptr;
  }

  return buf;
}
