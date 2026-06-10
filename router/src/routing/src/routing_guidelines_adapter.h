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

#ifndef ROUTING_GUIDELINES_ADAPTER_INCLUDED
#define ROUTING_GUIDELINES_ADAPTER_INCLUDED

#include <optional>
#include <string>
#include <string_view>
#include <system_error>

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif
#include <rapidjson/document.h>
#include <rapidjson/error/en.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/stringbuffer.h>

#include "mysql/harness/config_parser.h"
#include "mysql/harness/net_ts/io_context.h"
#include "mysql/harness/stdx/expected.h"
#include "mysqlrouter/routing_export.h"
#include "mysqlrouter/uri.h"
#include "protocol/protocol.h"

/**
 * Create a routing guideline from Router configuration.
 *
 * @param sections Router configuration sections
 * @param io_ctx IO context
 */
stdx::expected<std::string, std::error_code> ROUTING_EXPORT
create_routing_guidelines_document(
    const mysql_harness::Config::ConstSectionList &sections,
    net::io_context &io_ctx);

/**
 * Helper class used to create routing guideline from Router configuration.
 */
class Guidelines_from_conf_adapter {
 public:
  /**
   * Guidelines configuration adapter constructor.
   *
   * @param sections Router configuration sections
   * @param io_ctx IO context
   */
  Guidelines_from_conf_adapter(
      const mysql_harness::Config::ConstSectionList &sections,
      net::io_context &io_ctx);

  /**
   * Generate routing guideline based on the internal state of
   * Guidelines_from_conf_adapter.
   *
   * In case when there are only static routing plugins running the
   * guidelines document will be empty.
   */
  stdx::expected<std::string, std::error_code> generate_guidelines_string();

 private:
  const std::string kDefaultName{
      "Routing guidelines generated from a config file"};

  using JsonValue =
      rapidjson::GenericValue<rapidjson::UTF8<>, rapidjson::CrtAllocator>;
  using JsonDocument =
      rapidjson::GenericDocument<rapidjson::UTF8<>, rapidjson::CrtAllocator>;
  using JsonStringBuffer =
      rapidjson::GenericStringBuffer<rapidjson::UTF8<>,
                                     rapidjson::CrtAllocator>;

  /** Information about one routing section.*/
  struct Role_info {
    enum class Strategy {
      first_available,
      round_robin,
      round_robin_with_fallback
    };

    enum class Role { primary, secondary, primary_and_secondary };

    std::string role_str() const;
    std::string strategy_str() const;
    static Strategy strategy_from_string(std::string_view strategy_str);
    void set_strategy(const mysql_harness::ConfigSection *section);
    void set_protocol(const mysql_harness::ConfigSection *section);

    Role role_;
    Strategy strategy_;
    std::string host_;
    Protocol::Type protocol_;
  };

  /** Fill the internal routing guidelines doc.*/
  stdx::expected<void, std::error_code> fill_guidelines_doc();

  /** Add routing guidelines name section.*/
  void add_guidelines_name();

  /** Add routing guidelines version section.*/
  void add_guidelines_version();

  /** Add routing guidelines destinations section.*/
  void add_destinations(
      const std::string &section_name,
      const Guidelines_from_conf_adapter::Role_info &role_info);

  /** Add routing guidelines routes section.*/
  stdx::expected<void, std::error_code> add_routes(
      const std::string &section_name,
      const mysql_harness::ConfigSection *section,
      const Guidelines_from_conf_adapter::Role_info &role_info);

  /** Get detail info from one section. */
  std::optional<Role_info> get_role_info(
      const mysql_harness::ConfigSection *section) const;

  /** Create route match section. */
  stdx::expected<std::string, std::error_code> get_route_match(
      const mysql_harness::ConfigSection *section) const;

  /** If round-robin-with-fallback strategy is used get destination class that
   * could be used as a fallback. */
  std::optional<std::string> get_fallback_destination(
      const Protocol::Type protocol, std::string_view host) const;

  bool has_routes_{false};
  std::optional<std::string> fallback_src_;
  const mysql_harness::Config::ConstSectionList &sections_;
  net::io_context &io_ctx_;
  JsonDocument json_guidelines_doc_;
  rapidjson::CrtAllocator allocator_;
  JsonValue destinations_{rapidjson::kArrayType};
  JsonValue routes_{rapidjson::kArrayType};
};

#endif  // ROUTING_GUIDELINES_ADAPTER_INCLUDED
