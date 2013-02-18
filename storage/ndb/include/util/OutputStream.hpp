/*
   Copyright (C) 2003-2008 MySQL AB, 2008 Sun Microsystems, Inc.
    All rights reserved. Use is subject to license terms.

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

#ifndef OUTPUT_STREAM_HPP
#define OUTPUT_STREAM_HPP

#include <ndb_global.h>
#include <NdbTCP.h>

/**
 * Output stream
 */
class OutputStream {
public:
  OutputStream() {}
  virtual ~OutputStream() {}
  virtual int print(const char * fmt, ...)
    ATTRIBUTE_FORMAT(printf, 2, 3) = 0;
  virtual int println(const char * fmt, ...)
    ATTRIBUTE_FORMAT(printf, 2, 3) = 0;
  virtual int write(const void * buf, size_t len) = 0;
  virtual void flush() {};
  virtual void reset_timeout() {};
};

class FileOutputStream : public OutputStream {
  FILE * f;
public:
  FileOutputStream(FILE * file = stdout);
  virtual ~FileOutputStream() {}
  FILE *getFile() { return f; }

  int print(const char * fmt, ...)
    ATTRIBUTE_FORMAT(printf, 2, 3);
  int println(const char * fmt, ...)
    ATTRIBUTE_FORMAT(printf, 2, 3);
  int write(const void * buf, size_t len);
  void flush() { fflush(f); }
};

class SocketOutputStream : public OutputStream {
protected:
  NDB_SOCKET_TYPE m_socket;
  unsigned m_timeout_ms;
  bool m_timedout;
  unsigned m_timeout_remain;
public:
  SocketOutputStream(NDB_SOCKET_TYPE socket, unsigned write_timeout_ms = 1000);
  virtual ~SocketOutputStream() {}
  bool timedout() { return m_timedout; }
  void reset_timeout() { m_timedout= false; m_timeout_remain= m_timeout_ms;}

  int print(const char * fmt, ...)
    ATTRIBUTE_FORMAT(printf, 2, 3);
  int println(const char * fmt, ...)
    ATTRIBUTE_FORMAT(printf, 2, 3);
  int write(const void * buf, size_t len);
};


class BufferedSockOutputStream : public SocketOutputStream {
  class UtilBuffer& m_buffer;
public:
  BufferedSockOutputStream(NDB_SOCKET_TYPE socket,
                           unsigned write_timeout_ms = 1000);
  virtual ~BufferedSockOutputStream();

  int print(const char * fmt, ...)
    ATTRIBUTE_FORMAT(printf, 2, 3);
  int println(const char * fmt, ...)
    ATTRIBUTE_FORMAT(printf, 2, 3);

  int write(const void * buf, size_t len);
  void flush();
};


class NullOutputStream : public OutputStream {
public:
  NullOutputStream() {}
  virtual ~NullOutputStream() {}
  int print(const char * /* unused */, ...) { return 1;}
  int println(const char * /* unused */, ...) { return 1;}
  int write(const void * buf, size_t len) { return 1;}
};

#endif
