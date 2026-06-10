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

#ifndef _ROUTING_GUIDELINES_BUILDER_H_
#define _ROUTING_GUIDELINES_BUILDER_H_

#include <cstdint>
#include <string>
#include <vector>

namespace guidelines_builder {

struct Destination {
  std::string name;
  std::string match;
};

struct Route {
  struct Destination_list {
    std::string strategy;
    std::vector<std::string> destination_names;
    uint64_t priority{0};
  };

  std::string name;
  std::string match;
  std::vector<Destination_list> route_sinks;
  bool enabled{true};
  bool sharing_allowed{false};
};

std::string create(const std::vector<Destination> &destinations,
                   const std::vector<Route> &routes,
                   const std::string &name = "test_guidelines",
                   const std::string &version = "1.1");

}  // namespace guidelines_builder

#endif  // _ROUTING_GUIDELINES_BUILDER_H_
