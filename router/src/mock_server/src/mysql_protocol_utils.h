/*
  Copyright (c) 2017, 2018, Oracle and/or its affiliates. All rights reserved.

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
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef MYSQLD_MOCK_MYSQL_PROTOCOL_UTILS_INCLUDED
#define MYSQLD_MOCK_MYSQL_PROTOCOL_UTILS_INCLUDED

#include "mysql_protocol_decoder.h"
#include "mysql_protocol_encoder.h"

int get_socket_errno();
std::string get_socket_errno_str();
void send_packet(socket_t client_socket, const uint8_t *data, size_t size,
                 int flags = 0);
void send_packet(socket_t client_socket,
                 const server_mock::MySQLProtocolEncoder::MsgBuffer &buffer,
                 int flags = 0);
void read_packet(socket_t client_socket, uint8_t *data, size_t size,
                 int flags = 0);
int close_socket(socket_t sock);

#endif  // MYSQLD_MOCK_MYSQL_PROTOCOL_UTILS_INCLUDED
