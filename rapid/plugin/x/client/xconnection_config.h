/*
 * Copyright (c) 2017, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; version 2 of the
 * License.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301  USA
 */

// MySQL DB access module, for use by plugins and others
// For the module that implements interactive DB functionality see mod_db

#ifndef X_CLIENT_XCONNECTION_CONFIG_H_
#define X_CLIENT_XCONNECTION_CONFIG_H_

#include <cstdint>


namespace xcl {

class Connection_config {
 public:
  int64_t m_timeout_connect{ -1 };
  int64_t m_timeout_read{ -1 };
  int64_t m_timeout_write{ -1 };
};

}  // namespace xcl

#endif  // X_CLIENT_XCONNECTION_CONFIG_H_
