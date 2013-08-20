/* Copyright 2008 Sun Microsystems, Inc.
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
   Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA */

#ifndef SOCKETINPUTSTREAM2_HPP
#define SOCKETINPUTSTREAM2_HPP

#include <NdbTCP.h>
#include <BaseString.hpp>
#include <UtilBuffer.hpp>

class SocketInputStream2 {
  NDB_SOCKET_TYPE m_socket;
  unsigned m_read_timeout;
  UtilBuffer m_buffer;
  size_t m_buffer_read_pos;

  bool has_data_to_read();
  ssize_t read_socket(char* buf, size_t len);
  bool get_buffered_line(BaseString& str);
  bool add_buffer(char* buf, ssize_t len);

public:
  SocketInputStream2(NDB_SOCKET_TYPE socket,
                     unsigned read_timeout = 60) :
    m_socket(socket),
    m_read_timeout(read_timeout),
    m_buffer_read_pos(0)
    {};

  /*
    Read a line from socket into the string "str" until
    either terminating newline, EOF or read timeout encountered.

    Returns:
     true - a line ended with newline was read from socket
     false - EOF or read timeout occured

  */
  bool gets(BaseString& str);

};

#endif
