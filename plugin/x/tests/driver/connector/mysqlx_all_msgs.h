/*
 * Copyright (c) 2015, 2023, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */

#ifndef PLUGIN_X_TESTS_DRIVER_CONNECTOR_MYSQLX_ALL_MSGS_H_
#define PLUGIN_X_TESTS_DRIVER_CONNECTOR_MYSQLX_ALL_MSGS_H_

#include <cstdint>
#include <map>
#include <string>
#include <utility>

#include "plugin/x/client/mysqlxclient/xprotocol.h"

using Message_by_full_name = std::map<std::string, std::string>;

template <typename Message_id>
using Message_by_name =
    std::map<std::string,
             std::pair<xcl::XProtocol::Message *(*)(), Message_id>>;

using Message_server_by_name =
    Message_by_name<xcl::XProtocol::Server_message_type_id>;

using Message_client_by_name =
    Message_by_name<xcl::XProtocol::Client_message_type_id>;

template <typename Message_id>
using Message_by_id =
    std::map<Message_id,
             std::pair<xcl::XProtocol::Message *(*)(), std::string>>;

using Message_server_by_id =
    Message_by_id<xcl::XProtocol::Server_message_type_id>;

using Message_client_by_id =
    Message_by_id<xcl::XProtocol::Client_message_type_id>;

extern Message_by_full_name server_msgs_by_full_name;
extern Message_by_full_name client_msgs_by_full_name;

extern Message_server_by_name server_msgs_by_name;
extern Message_client_by_name client_msgs_by_name;

extern Message_server_by_id server_msgs_by_id;
extern Message_client_by_id client_msgs_by_id;

#endif  // PLUGIN_X_TESTS_DRIVER_CONNECTOR_MYSQLX_ALL_MSGS_H_
