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

#include "mrs/endpoint/db_schema_endpoint.h"

#include <mutex>

#include "mrs/endpoint/db_service_endpoint.h"
#include "mrs/observability/entity.h"
#include "mrs/router_observation_entities.h"

namespace mrs {
namespace endpoint {

using DbSchema = DbSchemaEndpoint::DbSchema;
using DbSchemaPtr = DbSchemaEndpoint::DbSchemaPtr;
using UniversalId = DbSchemaEndpoint::UniversalId;
using EnabledType = DbSchemaEndpoint::EnabledType;

DbSchemaEndpoint::DbSchemaEndpoint(const DbSchema &entry,
                                   EndpointConfigurationPtr configuration,
                                   HandlerFactoryPtr factory)
    : OptionEndpoint(entry.service_id, configuration, factory),
      entry_{std::make_shared<DbSchema>(entry)} {
  log_debug("DbSchemaEndpoint::DbSchemaEndpoint");
}

void DbSchemaEndpoint::activate_public() {
  url_handlers_.clear();

  url_handlers_.push_back(
      factory_->create_db_schema_metadata_catalog_handler(shared_from_this()));
  url_handlers_.push_back(
      factory_->create_db_schema_metadata_handler(shared_from_this()));
  url_handlers_.push_back(
      factory_->create_db_schema_openapi_handler(shared_from_this()));
}

void DbSchemaEndpoint::update() {
  Parent::update();
  observability::EntityCounter<kEntityCounterUpdatesSchemas>::increment();
}

void DbSchemaEndpoint::deactivate() { url_handlers_.clear(); }

UniversalId DbSchemaEndpoint::get_id() const { return entry_->id; }

UniversalId DbSchemaEndpoint::get_parent_id() const {
  return entry_->service_id;
}

const DbSchemaPtr DbSchemaEndpoint::get() const { return entry_; }

void DbSchemaEndpoint::set(const DbSchema &entry, EndpointBasePtr parent) {
  auto lock = std::unique_lock<std::shared_mutex>(endpoints_access_);
  entry_ = std::make_shared<DbSchema>(entry);
  change_parent(parent);
  changed();
}

EnabledType DbSchemaEndpoint::get_this_node_enabled_level() const {
  return entry_->enabled;
}

bool DbSchemaEndpoint::does_this_node_require_authentication() const {
  return entry_->requires_auth;
}

std::string DbSchemaEndpoint::get_my_url_path_part() const {
  return entry_->request_path;
}
std::string DbSchemaEndpoint::get_my_url_part() const {
  return entry_->request_path;
}

std::optional<std::string> DbSchemaEndpoint::get_options() const {
  return entry_->options;
}

}  // namespace endpoint
}  // namespace mrs
