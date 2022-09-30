/*
  Copyright (c) 2022, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_INTERFACE_ROUTE_FACTORY_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_INTERFACE_ROUTE_FACTORY_H_

#include <memory>

#include "mrs/database/entry/content_file.h"
#include "mrs/database/entry/db_object.h"
#include "mrs/interface/auth_manager.h"
#include "mrs/interface/handler_factory.h"
#include "mrs/interface/route.h"
#include "mrs/interface/route_manager.h"
#include "mrs/interface/route_schema.h"

namespace mrs {

namespace interface {

class RouteFactory {
 public:
  using Route = mrs::interface::Route;
  using RouteSchema = mrs::interface::RouteSchema;
  using ContentFile = mrs::database::entry::ContentFile;
  using DbObject = mrs::database::entry::DbObject;
  using AuthManager = mrs::interface::AuthManager;
  using RouteManager = mrs::interface::RouteManager;
  using MysqlCacheManager = collector::MysqlCacheManager;

 public:
  virtual ~RouteFactory() = default;

  virtual std::shared_ptr<Route> create_router_object(
      const DbObject &pe, std::shared_ptr<RouteSchema> schema,
      MysqlCacheManager *cache, const bool is_ssl,
      AuthManager *auth_manager) = 0;
  virtual std::shared_ptr<Route> create_router_static_object(
      const ContentFile &pe, std::shared_ptr<RouteSchema> schema,
      MysqlCacheManager *cache, const bool is_ssl,
      AuthManager *auth_manager) = 0;

  virtual std::shared_ptr<RouteSchema> create_router_schema(
      RouteManager *manager, MysqlCacheManager *cache,
      const std::string &service, const std::string &name, const bool is_ssl,
      const std::string &host, const bool requires_authentication,
      const uint64_t service_id, const uint64_t schema_id,
      AuthManager *auth_manager) = 0;
};

}  // namespace interface

}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_INTERFACE_ROUTE_FACTORY_H_
