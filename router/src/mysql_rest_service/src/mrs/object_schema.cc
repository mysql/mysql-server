/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#include "mrs/object_schema.h"

#include "mysql/harness/logging/logging.h"
#include "mysqlrouter/rest_api_utils.h"

#include "mrs/database/helper/query_table_columns.h"
#include "mrs/object_manager.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {

using VectorOfRoutes = ObjectSchema::VectorOfRoutes;

ObjectSchema::ObjectSchema(
    DbObjectManager *manager, collector::MysqlCacheManager *cache,
    const std::string &service, const std::string &name, const bool is_ssl,
    const std::string &host, const bool requires_authentication,
    const UniversalId &service_id, const UniversalId &schema_id,
    const std::string &options, mrs::interface::AuthorizeManager *auth_manager,
    std::shared_ptr<HandlerFactory> handler_factory)
    : manager_{manager},
      service_{service},
      name_{name},
      options_{options},
      cache_{cache},
      requires_authentication_{requires_authentication},
      service_id_{service_id},
      schema_id_{schema_id},
      auth_manager_{auth_manager},
      handler_factory_{handler_factory} {
  url_path_ = "^" + service_ + name_ + "/metadata-catalog/?$";
  url_ = service_ + name_ + "/metadata-catalog";
  if (!host.empty()) url_ = (is_ssl ? "https://" : "http://") + host + url_;
}

void ObjectSchema::turn(const State state) {
  if (state_ == state) return;
  state_ = state;

  if (stateOff == state) {
    rest_handler_schema_.reset();
    return;
  }

  if (!rest_handler_schema_) {
    rest_handler_schema_ =
        handler_factory_->create_schema_metadata_handler(this, auth_manager_);
  }
}

void ObjectSchema::route_unregister(Route *r) {
  auto i = std::find(routes_.begin(), routes_.end(), r);

  if (routes_.end() != i) {
    routes_.erase(i);
  }

  if (routes_.empty()) {
    manager_->schema_not_used(this);
  }
}

void ObjectSchema::route_register(Route *r) {
  if (std::find(routes_.begin(), routes_.end(), r) == routes_.end())
    routes_.push_back(r);
}

const std::string &ObjectSchema::get_path() const { return url_path_; }

const std::string &ObjectSchema::get_name() const { return name_; }

const std::string &ObjectSchema::get_options() const { return options_; }

const std::string ObjectSchema::get_full_path() const {
  return service_ + name_;
}

const std::string &ObjectSchema::get_url() const { return url_; }

const VectorOfRoutes &ObjectSchema::get_routes() const { return routes_; }

bool ObjectSchema::requires_authentication() const {
  return requires_authentication_;
}

UniversalId ObjectSchema::get_service_id() const { return service_id_; }

UniversalId ObjectSchema::get_id() const { return schema_id_; }

}  // namespace mrs
