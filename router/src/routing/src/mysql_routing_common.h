/*
  Copyright (c) 2018, 2022, Oracle and/or its affiliates.

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

#ifndef ROUTING_MYSQLROUTING_COMMON_INCLUDED
#define ROUTING_MYSQLROUTING_COMMON_INCLUDED

#include <string>

/**
 * @brief return a short string suitable to be used as a thread name
 *
 * @param config_name configuration name (e.g: "routing",
 * "routing:test_default_x_ro", etc)
 * @param prefix thread name prefix (e.g. "RtS")
 *
 * @return a short string (example: "RtS:x_ro")
 */
std::string get_routing_thread_name(const std::string &config_name,
                                    const std::string &prefix);

#endif /* ROUTING_MYSQLROUTING_COMMON_INCLUDED */
