/*
   Copyright (c) 2003, 2022, Oracle and/or its affiliates.

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
#include "portlib/ndb_compiler.h"
#include "portlib/ndb_socket.h"

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
  virtual void flush() {}
  virtual void reset_timeout() {}
};

/**
 * Strings going to BufferedOutputStream are appended to
 * a LogBuffer object which are later retrieved by a log
 * thread and written to the log file.
 */

class BufferedOutputStream : public OutputStream {
public:
  BufferedOutputStream(class LogBuffer* plogBuf);
  ~BufferedOutputStream() override {}

  int print(const char * fmt, ...) override
    ATTRIBUTE_FORMAT(printf, 2, 3);
  int println(const char * fmt, ...) override
    ATTRIBUTE_FORMAT(printf, 2, 3);
  int write(const void * buf, size_t len) override;
  void flush() override {}

private:
  class LogBuffer* logBuf;
};

class FileOutputStream : public OutputStream {
  FILE * f;
public:
  FileOutputStream(FILE * file = stdout);
  ~FileOutputStream() override {}
  FILE *getFile() { return f; }

  int print(const char * fmt, ...) override
    ATTRIBUTE_FORMAT(printf, 2, 3);
  int println(const char * fmt, ...) override
    ATTRIBUTE_FORMAT(printf, 2, 3);
  int write(const void * buf, size_t len) override;
  void flush() override { fflush(f); }
};

class SocketOutputStream : public OutputStream {
protected:
  ndb_socket_t m_socket;
  unsigned m_timeout_ms;
  bool m_timedout;
  unsigned m_timeout_remain;
public:
  SocketOutputStream(ndb_socket_t socket, unsigned write_timeout_ms = 1000);
  ~SocketOutputStream() override {}
  bool timedout() { return m_timedout; }
  void reset_timeout() override { m_timedout= false; m_timeout_remain= m_timeout_ms;}

  int print(const char * fmt, ...) override
    ATTRIBUTE_FORMAT(printf, 2, 3);
  int println(const char * fmt, ...) override
    ATTRIBUTE_FORMAT(printf, 2, 3);
  int write(const void * buf, size_t len) override;
};


class BufferedSockOutputStream : public SocketOutputStream {
  class UtilBuffer& m_buffer;
public:
  BufferedSockOutputStream(ndb_socket_t socket,
                           unsigned write_timeout_ms = 1000);
  ~BufferedSockOutputStream() override;

  int print(const char * fmt, ...) override
    ATTRIBUTE_FORMAT(printf, 2, 3);
  int println(const char * fmt, ...) override
    ATTRIBUTE_FORMAT(printf, 2, 3);

  int write(const void * buf, size_t len) override;
  void flush() override;
};


class NullOutputStream : public OutputStream {
public:
  NullOutputStream() {}
  ~NullOutputStream() override {}
  int print(const char * /* unused */, ...) override { return 1;}
  int println(const char * /* unused */, ...) override { return 1;}
  int write(const void * /*buf*/, size_t /*len*/) override { return 1;}
};

class StaticBuffOutputStream : public OutputStream
{
private:
  char* m_buff;
  const size_t m_size;
  size_t m_offset;
public:
  StaticBuffOutputStream(char* buff, size_t size);
  ~StaticBuffOutputStream() override;

  int print(const char * fmt, ...) override
    ATTRIBUTE_FORMAT(printf, 2, 3);
  int println(const char * fmt, ...) override
    ATTRIBUTE_FORMAT(printf, 2, 3);

  int write(const void * buf, size_t len) override;
  void flush() override {}

  const char* getBuff() const {return m_buff;}
  size_t getLen() const {return m_offset;}
  void reset() {m_buff[0] = '\n'; m_offset = 0; }
};

#endif
