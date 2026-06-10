/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#ifndef MYSQL_ROUTER_ROUTING_DESTINATIONS_OPTION_PARSER_H
#define MYSQL_ROUTER_ROUTING_DESTINATIONS_OPTION_PARSER_H

#include <string>
#include <variant>
#include <vector>

#include "mysql/harness/destination.h"
#include "mysql/harness/stdx/expected.h"
#include "mysqlrouter/routing_export.h"  // ROUTING_EXPORT
#include "mysqlrouter/uri.h"

class ROUTING_EXPORT DestinationsOptionParser {
 public:
  /**
   * parse the destination string and either return a metadata-cache URI or a
   * vector of destinations.
   */
  [[nodiscard]] static stdx::expected<
      std::variant<mysqlrouter::URI, std::vector<mysql_harness::Destination>>,
      std::string>
  parse(const std::string &value);
};

#endif
