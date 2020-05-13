/*
  Copyright (c) 2019, 2020, Oracle and/or its affiliates. All rights reserved.

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

#include "mysql/harness/net_ts/socket.h"

// materialize constexpr's

constexpr net::socket_base::wait_type net::socket_base::wait_write;
constexpr net::socket_base::wait_type net::socket_base::wait_read;
constexpr net::socket_base::wait_type net::socket_base::wait_error;

constexpr net::socket_base::shutdown_type net::socket_base::shutdown_receive;
constexpr net::socket_base::shutdown_type net::socket_base::shutdown_send;
constexpr net::socket_base::shutdown_type net::socket_base::shutdown_both;

constexpr net::socket_base::message_flags net::socket_base::message_peek;
constexpr net::socket_base::message_flags
    net::socket_base::message_do_not_route;
constexpr net::socket_base::message_flags net::socket_base::message_out_of_band;
