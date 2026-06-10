/*
 * Copyright (c) 2024, 2026, Oracle and/or its affiliates.
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
 * This program is distributed in the hope that it will be useful,  but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See
 * the GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin St, Fifth Floor, Boston, MA 02110-1301 USA
 */

#ifndef ROUTER_SRC_ROUTING_GUIDELINES_SRC_ROUTING_SIMULATOR_H_
#define ROUTER_SRC_ROUTING_GUIDELINES_SRC_ROUTING_SIMULATOR_H_

#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>
#include <rapidjson/error/en.h>

#include "routing_guidelines/routing_guidelines.h"
#include "rpn.h"

namespace routing_guidelines {

class Routing_simulator {
 public:
  explicit Routing_simulator(
      rpn::Context *external_context = nullptr,
      Routing_guidelines_engine::ResolveCache *external_cache = nullptr);

  stdx::expected<void, std::string> process_document(const std::string &s);

 private:
  stdx::expected<void, std::string> parse_router(const rapidjson::Document &d);
  stdx::expected<void, std::string> parse_destination(
      const rapidjson::Document &d, std::string_view expected_name);
  stdx::expected<void, std::string> parse_source(
      const rapidjson::Document &d, std::string_view expected_name);
  stdx::expected<void, std::string> parse_sql(const rapidjson::Document &d,
                                              std::string_view expected_name);

  std::unique_ptr<Routing_guidelines_engine> rpd_;

  // External context + variables to keep it valid
  rpn::Context *external_context_{nullptr};
  Routing_guidelines_engine::ResolveCache *external_cache_{nullptr};
  Router_info router_;
  Session_info session_;
  Sql_info sql_;
  Server_info server_;

  // Tracking many destinations and sources
  std::unordered_map<std::string, Server_info> destinations_;
  std::string last_destination_;
  std::unordered_map<int, Session_info> sources_;
  int64_t last_source_{-1};
};

}  // namespace routing_guidelines

#endif  // ROUTER_SRC_ROUTING_GUIDELINES_SRC_ROUTING_SIMULATOR_H_
