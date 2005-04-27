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
				       unsigned timeout){
  m_socket = socket;
  m_timeout = timeout;
}

int
SocketOutputStream::print(const char * fmt, ...){
  va_list ap;
  va_start(ap, fmt);
  const int ret = vprint_socket(m_socket, m_timeout, fmt, ap);
  va_end(ap);
  return ret;
}
int
SocketOutputStream::println(const char * fmt, ...){
  va_list ap;
  va_start(ap, fmt);
  const int ret = vprintln_socket(m_socket, m_timeout, fmt, ap);
  va_end(ap);
  return ret;
}

#ifdef NDB_SOFTOSE
#include <dbgprintf.h>
int
SoftOseOutputStream::print(const char * fmt, ...){
  va_list ap;
  char buf[1000];
  
  va_start(ap, fmt);
  if (fmt != 0)
    BaseString::vsnprintf(buf, sizeof(buf)-1, fmt, ap);
  else
    buf[0] = 0;
  va_end(ap);
  dbgprintf(buf);
}

int
SoftOseOutputStream::println(const char * fmt, ...){
  va_list ap;
  char buf[1000];
  
  va_start(ap, fmt);
  if (fmt != 0)
    BaseString::vsnprintf(buf, sizeof(buf)-1, fmt, ap);
  else
    buf[0] = 0;
  va_end(ap);
  
  strcat(buf, "\n\r");
  dbgprintf(buf);
}
#endif
