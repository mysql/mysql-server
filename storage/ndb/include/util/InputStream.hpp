/*
   Copyright (C) 2003-2008 MySQL AB
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

#ifndef INPUT_STREAM_HPP
#define INPUT_STREAM_HPP

#include <ndb_global.h>
#include <NdbTCP.h>
#include <NdbMutex.h>

/**
 * Input stream
 */
class InputStream {
public:
  InputStream() { m_mutex= NULL; }
  virtual ~InputStream() {}
  virtual char* gets(char * buf, int bufLen) = 0;
  /**
   * Set the mutex to be UNLOCKED when blocking (e.g. select(2))
   */
  void set_mutex(NdbMutex *m) { m_mutex= m; }
  virtual void reset_timeout() {}
protected:
  NdbMutex *m_mutex;
};

class FileInputStream : public InputStream {
  FILE * f;
public:
  FileInputStream(FILE * file = stdin);
  virtual ~FileInputStream() {}
  char* gets(char * buf, int bufLen); 
};

extern FileInputStream Stdin;

class SocketInputStream : public InputStream {
  NDB_SOCKET_TYPE m_socket;
  unsigned m_timeout_ms;
  unsigned m_timeout_remain;
  bool m_startover;
  bool m_timedout;
public:
  SocketInputStream(NDB_SOCKET_TYPE socket, unsigned read_timeout_ms = 3000);
  virtual ~SocketInputStream() {}
  char* gets(char * buf, int bufLen);
  bool timedout() { return m_timedout; }
  void reset_timeout() { m_timedout= false; m_timeout_remain= m_timeout_ms;}

};

#endif
