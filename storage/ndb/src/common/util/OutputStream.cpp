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

#include <OutputStream.hpp>
#include <socket_io.h>

FileOutputStream::FileOutputStream(FILE * file){
  f = file;
}

int
FileOutputStream::print(const char * fmt, ...){
  va_list ap;
  va_start(ap, fmt);
  const int ret = vfprintf(f, fmt, ap);
  va_end(ap);
  return ret;
}

int
FileOutputStream::println(const char * fmt, ...){
  va_list ap;
  va_start(ap, fmt);
  const int ret = vfprintf(f, fmt, ap);
  va_end(ap);
  return ret + fprintf(f, "\n");
}

SocketOutputStream::SocketOutputStream(NDB_SOCKET_TYPE socket,
				       unsigned write_timeout_ms){
  m_socket = socket;
  m_timeout_remain= m_timeout_ms = write_timeout_ms;
  m_timedout= false;
}

int
SocketOutputStream::print(const char * fmt, ...){
  va_list ap;

  if(timedout())
    return -1;

  int time= 0;
  va_start(ap, fmt);
  int ret = vprint_socket(m_socket, m_timeout_ms, &time, fmt, ap);
  va_end(ap);

  if(ret >= 0)
    m_timeout_remain-=time;
  if((ret < 0 && errno==ETIMEDOUT) || m_timeout_remain<=0)
  {
    m_timedout= true;
    ret= -1;
  }

  return ret;
}
int
SocketOutputStream::println(const char * fmt, ...){
  va_list ap;

  if(timedout())
    return -1;

  int time= 0;
  va_start(ap, fmt);
  int ret = vprintln_socket(m_socket, m_timeout_ms, &time, fmt, ap);
  va_end(ap);

  if(ret >= 0)
    m_timeout_remain-=time;
  if ((ret < 0 && errno==ETIMEDOUT) || m_timeout_remain<=0)
  {
    m_timedout= true;
    ret= -1;
  }

  return ret;
}
