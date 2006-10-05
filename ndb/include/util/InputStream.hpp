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

#ifndef INPUT_STREAM_HPP
#define INPUT_STREAM_HPP

#include <ndb_global.h>
#include <NdbTCP.h>

/**
 * Input stream
 */
class InputStream {
public:
  virtual char* gets(char * buf, int bufLen) = 0;
};

class FileInputStream : public InputStream {
  FILE * f;
public:
  FileInputStream(FILE * file = stdin);
  char* gets(char * buf, int bufLen); 
};

extern FileInputStream Stdin;

class SocketInputStream : public InputStream {
  NDB_SOCKET_TYPE m_socket;
  unsigned m_timeout;
  bool m_startover;
public:
  SocketInputStream(NDB_SOCKET_TYPE socket, unsigned readTimeout = 1000);
  char* gets(char * buf, int bufLen);
};

#endif
