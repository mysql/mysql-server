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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_ENDPOINT_DB_SCHEMA_ENDPOINT_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_ENDPOINT_DB_SCHEMA_ENDPOINT_H_

#include <memory>

#include "mrs/database/entry/db_schema.h"
#include "mrs/endpoint/option_endpoint.h"
#include "mrs/interface/handler_factory.h"
#include "mrs/interface/rest_handler.h"

namespace mrs {
namespace endpoint {

class DbServiceEndpoint;

class DbSchemaEndpoint : public OptionEndpoint {
 public:
  using Parent = OptionEndpoint;
  using DbSchema = mrs::database::entry::DbSchema;
  using DbSchemaPtr = std::shared_ptr<DbSchema>;
  using HandlerFactoryPtr = std::shared_ptr<mrs::interface::HandlerFactory>;
  using DataType = DbSchema;

 public:
  DbSchemaEndpoint(const DbSchema &entry,
                   EndpointConfigurationPtr configuration,
                   HandlerFactoryPtr factory);

  void activate_public() override;
  void deactivate() override;

  UniversalId get_id() const override;
  UniversalId get_parent_id() const override;
  std::optional<std::string> get_options() const override;

  void set(const DbSchema &schema, EndpointBasePtr parent);
  const DbSchemaPtr get() const;

 private:
  void update() override;
  EnabledType get_this_node_enabled_level() const override;
  bool does_this_node_require_authentication() const override;
  std::string get_my_url_path_part() const override;
  std::string get_my_url_part() const override;

  DbSchemaPtr entry_;
  std::vector<HandlerPtr> url_handlers_;
};

}  // namespace endpoint

}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_ENDPOINT_DB_SCHEMA_ENDPOINT_H_
