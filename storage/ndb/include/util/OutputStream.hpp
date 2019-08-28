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

/**
 * Strings going to BufferedOutputStream are appended to
 * a LogBuffer object which are later retrieved by a log
 * thread and written to the log file.
 */

class BufferedOutputStream : public OutputStream {
public:
  BufferedOutputStream(class LogBuffer* plogBuf);
  virtual ~BufferedOutputStream() {}

  int print(const char * fmt, ...)
    ATTRIBUTE_FORMAT(printf, 2, 3);
  int println(const char * fmt, ...)
    ATTRIBUTE_FORMAT(printf, 2, 3);
  int write(const void * buf, size_t len);
  void flush() {};

private:
  class LogBuffer* logBuf;
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
