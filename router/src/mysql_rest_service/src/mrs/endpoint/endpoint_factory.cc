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

#include "mrs/endpoint/endpoint_factory.h"

#include "helper/typeid_name.h"
#include "mrs/endpoint/content_file_endpoint.h"
#include "mrs/endpoint/content_set_endpoint.h"
#include "mrs/endpoint/db_object_endpoint.h"
#include "mrs/endpoint/db_schema_endpoint.h"
#include "mrs/endpoint/db_service_endpoint.h"
#include "mrs/endpoint/url_host_endpoint.h"

namespace mrs {
namespace endpoint {

using EndpointBasePtr = EndpointFactory::EndpointBasePtr;

namespace {
bool g_logging = true;
}  // namespace

template <typename Type>
class LogCreation : public Type {
 public:
  template <typename... Args>
  LogCreation(Args &&...args) : Type(std::forward<Args>(args)...) {
    log_debug("ctor endpoint: %s", helper::type_name<Type>().c_str());
  }
  ~LogCreation() override {
    log_debug("dtor endpoint: %s", helper::type_name<Type>().c_str());
  }
};

template <typename Endpoint, typename... Args>
EndpointBasePtr make_endoint(Args &&...args) {
  if (!g_logging)
    return std::make_shared<Endpoint>(std::forward<Args>(args)...);

  return std::make_shared<LogCreation<Endpoint>>(std::forward<Args>(args)...);
}

EndpointBasePtr EndpointFactory::create_object(const ContentSet &set,
                                               EndpointBasePtr parent) {
  auto result = make_endoint<mrs::endpoint::ContentSetEndpoint>(
      set, configuration_, handler_factory_);
  // Generate `update` with new parent, call after object is fully created.
  result->set_parent(parent);
  return result;
}

EndpointBasePtr EndpointFactory::create_object(const ContentFile &file,
                                               EndpointBasePtr parent) {
  auto result = make_endoint<mrs::endpoint::ContentFileEndpoint>(
      file, configuration_, handler_factory_);
  // Generate `update` with new parent, call after object is fully created.
  result->set_parent(parent);
  return result;
}

EndpointBasePtr EndpointFactory::create_object(const DbSchema &schema,
                                               EndpointBasePtr parent) {
  auto result = make_endoint<mrs::endpoint::DbSchemaEndpoint>(
      schema, configuration_, handler_factory_);
  // Generate `update` with new parent, call after object is fully created.
  result->set_parent(parent);
  return result;
}

EndpointBasePtr EndpointFactory::create_object(const DbObject &obj,
                                               EndpointBasePtr parent) {
  auto result = make_endoint<mrs::endpoint::DbObjectEndpoint>(
      obj, configuration_, handler_factory_);
  // Generate `update` with new parent, call after object is fully created.
  result->set_parent(parent);
  return result;
}

EndpointBasePtr EndpointFactory::create_object(const DbService &service,
                                               EndpointBasePtr parent) {
  auto result = make_endoint<mrs::endpoint::DbServiceEndpoint>(
      service, configuration_, handler_factory_);
  // Generate `update` with new parent, call after object is fully created.
  result->set_parent(parent);
  return result;
}

EndpointBasePtr EndpointFactory::create_object(const UrlHost &host,
                                               EndpointBasePtr parent) {
  auto result = make_endoint<mrs::endpoint::UrlHostEndpoint>(
      host, configuration_, handler_factory_);
  // Generate `update` with new parent, call after object is fully created.
  result->set_parent(parent);
  return result;
}

}  // namespace endpoint
}  // namespace mrs
