/*
   Copyright (c) 2003, 2017, Oracle and/or its affiliates. All rights reserved.

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

#include <OutputStream.hpp>
#include <socket_io.h>
#include <LogBuffer.hpp>
#include <BaseString.hpp>

BufferedOutputStream::BufferedOutputStream(LogBuffer* plogBuf){
  logBuf = plogBuf;
  assert(logBuf != NULL);
}

int
BufferedOutputStream::print(const char * fmt, ...){
  char buf[1];
  va_list ap;
  int len = 0;
  int ret = 0;

  va_start(ap, fmt);
  len = BaseString::vsnprintf(buf, sizeof(buf), fmt, ap);
  assert(len >= 0);
  va_end(ap);

  va_start(ap, fmt);
  ret = logBuf->append(fmt, ap, (size_t)len);
  va_end(ap);
  return ret;
}

int
BufferedOutputStream::println(const char * fmt, ...){
  char buf[1];
  va_list ap;
  int len = 0;
  int ret = 0;

  va_start(ap, fmt);
  len = BaseString::vsnprintf(buf, sizeof(buf), fmt, ap);
  assert(len >= 0);
  va_end(ap);

  va_start(ap, fmt);
  ret = logBuf->append(fmt, ap, (size_t)len, true);
  va_end(ap);
  return ret;
}

int
BufferedOutputStream::write(const void * buf, size_t len)
{
  return (int)(logBuf->append((void*)buf, len));
}

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

int
FileOutputStream::write(const void * buf, size_t len)
{
  return (int)fwrite(buf, len, 1, f);
}

SocketOutputStream::SocketOutputStream(NDB_SOCKET_TYPE socket,
				       unsigned write_timeout_ms) :
  m_socket(socket),
  m_timeout_ms(write_timeout_ms),
  m_timedout(false),
  m_timeout_remain(write_timeout_ms)
{
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
  if((ret < 0 && errno==SOCKET_ETIMEDOUT) || m_timeout_remain<=0)
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
  if ((ret < 0 && errno==SOCKET_ETIMEDOUT) || m_timeout_remain<=0)
  {
    m_timedout= true;
    ret= -1;
  }

  return ret;
}

int
SocketOutputStream::write(const void * buf, size_t len)
{
  if (timedout())
    return -1;

  int time = 0;
  int ret = write_socket(m_socket, m_timeout_ms, &time,
                         (const char*)buf, (int)len);
  if (ret >= 0)
  {
    m_timeout_remain -= time;
  }

  if ((ret < 0 && errno == SOCKET_ETIMEDOUT) || m_timeout_remain <= 0)
  {
    m_timedout = true;
    ret= -1;
  }
  return ret;
}

#include <UtilBuffer.hpp>

BufferedSockOutputStream::BufferedSockOutputStream(NDB_SOCKET_TYPE socket,
                                                   unsigned write_timeout_ms) :
  SocketOutputStream(socket, write_timeout_ms),
  m_buffer(*new UtilBuffer)
{
}

BufferedSockOutputStream::~BufferedSockOutputStream()
{
  delete &m_buffer;
}

int
BufferedSockOutputStream::print(const char * fmt, ...){
  char buf[1];
  va_list ap;
  int len;
  char* pos;

  // Find out length of string
  va_start(ap, fmt);
  len = BaseString::vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  // Allocate a temp buffer for the string
  UtilBuffer tmp;
  if (tmp.append(len+1) == 0)
    return -1;

  // Print to temp buffer
  va_start(ap, fmt);
  len = BaseString::vsnprintf((char*)tmp.get_data(), len+1, fmt, ap);
  va_end(ap);

  // Grow real buffer so it can hold the string
  if ((pos= (char*)m_buffer.append(len)) == 0)
    return -1;

  // Move everything except ending 0 to real buffer
  memcpy(pos, tmp.get_data(), tmp.length()-1);

  return 0;
}

int
BufferedSockOutputStream::println(const char * fmt, ...){
  char buf[1];
  va_list ap;
  int len;
  char* pos;

  // Find out length of string
  va_start(ap, fmt);
  len = BaseString::vsnprintf(buf, sizeof(buf), fmt, ap);
  va_end(ap);

  // Grow buffer so it can hold the string and the new line
  if ((pos= (char*)m_buffer.append(len+1)) == 0)
    return -1;

  // Print string to buffer
  va_start(ap, fmt);
  len = BaseString::vsnprintf((char*)pos, len+1, fmt, ap);
  va_end(ap);

  // Add newline
  pos+= len;
  *pos= '\n';

  return 0;
}

int
BufferedSockOutputStream::write(const void * buf, size_t len)
{
  return m_buffer.append(buf, len);
}

void BufferedSockOutputStream::flush(){
  int elapsed = 0;
  if (write_socket(m_socket, m_timeout_ms, &elapsed,
                   (const char*)m_buffer.get_data(), m_buffer.length()) != 0)
  {
    fprintf(stderr, "Failed to flush buffer to socket, errno: %d\n", errno);
  }

  m_buffer.clear();
}

