/*
   Copyright (C) 2003-2008 MySQL AB
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
