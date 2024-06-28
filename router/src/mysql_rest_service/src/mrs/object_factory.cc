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

#include "mrs/object_factory.h"

#include <memory>

#include "mrs/database/query_factory.h"
#include "mrs/object.h"
#include "mrs/object_schema.h"
#include "mrs/object_static_file.h"
#include "mrs/rest/handler_factory.h"

namespace mrs {

ObjectFactory::ObjectFactory(HandlerFactory *handler_factory,
                             QueryFactory *query_factory)
    : handler_factory_{handler_factory}, query_factory_{query_factory} {}

std::shared_ptr<ObjectFactory::Object> ObjectFactory::create_router_object(
    const DbObject &pe, std::shared_ptr<ObjectSchema> schema,
    collector::MysqlCacheManager *cache, const bool is_ssl,
    mrs::interface::AuthorizeManager *auth_manager, GtidManager *gtid_manager) {
  return std::make_shared<mrs::Object>(pe, schema, cache, is_ssl, auth_manager,
                                       gtid_manager, handler_factory_,
                                       query_factory_);
}

std::shared_ptr<ObjectFactory::Object>
ObjectFactory::create_router_static_object(
    const ContentFile &pe, std::shared_ptr<ObjectSchema> schema,
    collector::MysqlCacheManager *cache, const bool is_ssl,
    mrs::interface::AuthorizeManager *auth_manager) {
  return std::make_shared<mrs::ObjectStaticFile>(pe, schema, cache, is_ssl,
                                                 auth_manager, handler_factory_,
                                                 query_factory_);
}

std::shared_ptr<ObjectFactory::ObjectSchema>
ObjectFactory::create_router_schema(
    DbObjectManager *manager, collector::MysqlCacheManager *cache,
    const std::string &service, const std::string &name, const bool is_ssl,
    const std::string &host, const bool requires_authentication,
    const UniversalId service_id, const UniversalId schema_id,
    const std::string &options,
    mrs::interface::AuthorizeManager *auth_manager) {
  return std::make_shared<mrs::ObjectSchema>(
      manager, cache, service, name, is_ssl, host, requires_authentication,
      service_id, schema_id, options, auth_manager, handler_factory_);
}

}  // namespace mrs
