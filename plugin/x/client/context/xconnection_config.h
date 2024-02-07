/*
 * Copyright (c) 2017, 2024, Oracle and/or its affiliates.
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

// MySQL DB access module, for use by plugins and others
// For the module that implements interactive DB functionality see mod_db

#ifndef PLUGIN_X_CLIENT_CONTEXT_XCONNECTION_CONFIG_H_
#define PLUGIN_X_CLIENT_CONTEXT_XCONNECTION_CONFIG_H_

#include <cstdint>
#include <limits>
#include <string>

namespace xcl {

class Connection_config {
 public:
  int64_t m_timeout_connect{-1};
  int64_t m_timeout_session_connect{-1};
  int64_t m_timeout_read{-1};
  int64_t m_timeout_write{-1};
  int64_t m_buffer_receive_size{64 * 1024};
  std::string m_network_namespace;
};

}  // namespace xcl

#endif  // PLUGIN_X_CLIENT_CONTEXT_XCONNECTION_CONFIG_H_
