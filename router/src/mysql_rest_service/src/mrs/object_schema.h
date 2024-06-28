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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_OBJECT_SCHEMA_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_OBJECT_SCHEMA_H_

#include <map>
#include <memory>
#include <string>
#include <vector>

#include "mysqlrouter/component/http_server_component.h"

#include "collector/mysql_cache_manager.h"
#include "mrs/interface/handler_factory.h"
#include "mrs/interface/object_manager.h"
#include "mrs/interface/object_schema.h"
#include "mrs/interface/rest_handler.h"
#include "mrs/interface/state.h"

namespace mrs {

class ObjectSchema : public std::enable_shared_from_this<ObjectSchema>,
                     public mrs::interface::ObjectSchema {
 public:
  using DbObjectManager = mrs::interface::ObjectManager;
  using Route = mrs::interface::Object;
  using VectorOfRoutes = std::vector<Route *>;
  using HandlerFactory = mrs::interface::HandlerFactory;
  using AuthorizeManager = mrs::interface::AuthorizeManager;

 public:
  ObjectSchema(DbObjectManager *manager, collector::MysqlCacheManager *cache,
               const std::string &service, const std::string &name,
               const bool is_ssl, const std::string &host,
               const bool requires_authentication,
               const UniversalId &service_id, const UniversalId &schema_id,
               const std::string &options,
               mrs::interface::AuthorizeManager *auth_manager,
               HandlerFactory *handler_factory);

  void turn(const State state) override;
  void route_unregister(Route *r) override;
  void route_register(Route *r) override;

  const std::string get_full_path() const override;
  const std::string &get_name() const override;
  const std::string &get_url() const override;
  const std::string &get_path() const override;
  const std::string &get_options() const override;
  const VectorOfRoutes &get_routes() const override;
  bool requires_authentication() const override;
  UniversalId get_service_id() const override;
  UniversalId get_id() const override;

 private:
  State state_{stateOff};
  DbObjectManager *manager_;
  std::string service_;
  std::string name_;
  std::string url_;
  std::string url_path_;
  std::string options_;
  collector::MysqlCacheManager *cache_;
  VectorOfRoutes routes_;
  std::unique_ptr<Handler> rest_handler_schema_;
  bool requires_authentication_;
  UniversalId service_id_;
  UniversalId schema_id_;
  AuthorizeManager *auth_manager_;
  HandlerFactory *handler_factory_;
};

}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_OBJECT_SCHEMA_H_
