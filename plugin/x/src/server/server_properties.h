/*
 * Copyright (c) 2018, 2024, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
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

#ifndef PLUGIN_X_SRC_SERVER_SERVER_PROPERTIES_H_
#define PLUGIN_X_SRC_SERVER_SERVER_PROPERTIES_H_

#include <map>
#include <string>

namespace ngs {

enum class Server_property_ids {
  k_was_prepared,
  k_number_of_interfaces,
  k_tcp_port,
  k_tcp_bind_address,
  k_unix_socket
};

using Server_properties = std::map<Server_property_ids, std::string>;

const char *const PROPERTY_NOT_CONFIGURED = "UNDEFINED";

}  // namespace ngs

#endif  // PLUGIN_X_SRC_SERVER_SERVER_PROPERTIES_H_
