/*
  Copyright (c) 2025, 2026, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is designed to work with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have either included with
  the program or referenced in the documentation.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTER_SRC_ROUTER_INCLUDE_MYSQLROUTER_CONFIG_UTILS_H_
#define ROUTER_SRC_ROUTER_INCLUDE_MYSQLROUTER_CONFIG_UTILS_H_

#include <cstdint>
#include <optional>
#include <string>

#include "mysqlrouter/router_utils_export.h"

namespace mysql_harness {
class Config;
}  // namespace mysql_harness

std::optional<std::string> ROUTER_UTILS_EXPORT get_configured_router_name(
    const mysql_harness::Config &config, const uint32_t default_port);

uint32_t ROUTER_UTILS_EXPORT get_configured_http_port(
    const mysql_harness::Config &config, const uint32_t default_port);

#endif  // ROUTER_SRC_ROUTER_INCLUDE_MYSQLROUTER_CONFIG_UTILS_H_
