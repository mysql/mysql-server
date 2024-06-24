/*
  Copyright (c) 2022, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_INTERFACE_OBJECT_FACTORY_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_INTERFACE_OBJECT_FACTORY_H_

#include <memory>

#include "mrs/database/entry/db_object.h"
#include "mrs/interface/authorize_manager.h"
#include "mrs/interface/handler_factory.h"
#include "mrs/interface/object_manager.h"
#include "mrs/interface/object_schema.h"
#include "mrs/rest/entry/app_content_file.h"

namespace mrs {

namespace interface {

class ObjectFactory {
 public:
  using Object = mrs::interface::Object;
  using ObjectSchema = mrs::interface::ObjectSchema;
  using ContentFile = mrs::rest::entry::AppContentFile;
  using DbObject = mrs::database::entry::DbObject;
  using AuthManager = mrs::interface::AuthorizeManager;
  using DbObjectManager = mrs::interface::ObjectManager;
  using MysqlCacheManager = collector::MysqlCacheManager;
  using GtidManager = mrs::GtidManager;

 public:
  virtual ~ObjectFactory() = default;

  virtual std::shared_ptr<Object> create_router_object(
      const DbObject &pe, std::shared_ptr<ObjectSchema> schema,
      MysqlCacheManager *cache, const bool is_ssl, AuthManager *auth_manager,
      GtidManager *gtid_manager) = 0;
  virtual std::shared_ptr<Object> create_router_static_object(
      const ContentFile &pe, std::shared_ptr<ObjectSchema> schema,
      MysqlCacheManager *cache, const bool is_ssl,
      AuthManager *auth_manager) = 0;

  virtual std::shared_ptr<ObjectSchema> create_router_schema(
      DbObjectManager *manager, MysqlCacheManager *cache,
      const std::string &service, const std::string &name, const bool is_ssl,
      const std::string &host, const bool requires_authentication,
      const UniversalId service_id, const UniversalId schema_id,
      const std::string &options, AuthManager *auth_manager) = 0;
};

}  // namespace interface

}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_INTERFACE_OBJECT_FACTORY_H_
