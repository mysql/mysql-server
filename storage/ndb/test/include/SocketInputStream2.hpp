/*
   Copyright (c) 2008, 2017, Oracle and/or its affiliates. All rights reserved.

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
