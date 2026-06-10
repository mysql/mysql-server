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
#include "handler_db_schema_openapi.h"

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>
#include <rapidjson/writer.h>

#include "mrs/endpoint/handler/helper/url_paths.h"
#include "mrs/endpoint/handler/helper/utilities.h"
#include "mrs/endpoint/handler/helper/utils_proto.h"
#include "mrs/http/error.h"
#include "mrs/rest/openapi_object_creator.h"
#include "mrs/rest/request_context.h"

namespace mrs {
namespace endpoint {
namespace handler {

using HttpResult = mrs::rest::Handler::HttpResult;
using Authorization = mrs::rest::Handler::Authorization;

namespace {

auto get_path_schema_openapi(std::weak_ptr<DbSchemaEndpoint> endpoint) {
  using namespace std::string_literals;
  std::vector<::http::base::UriPathMatcher> result;

  auto endpoint_sch = lock(endpoint);
  if (!endpoint_sch) return result;

  result.push_back(path_schema_openapi_swagger(endpoint_sch->get_url_path()));

  const auto path = endpoint_sch->get_url_path();
  size_t slash_pos = path.find('/', 1);  // skip initial '/'
  if (slash_pos == std::string::npos) return result;

  const std::string service_name = path.substr(1, slash_pos - 1);
  const std::string schema_name = path.substr(slash_pos + 1);

  result.push_back(
      path_schema_openapi_swagger_alias(service_name, schema_name));

  return result;
}

}  // namespace

HandlerDbSchemaOpenAPI::HandlerDbSchemaOpenAPI(
    std::weak_ptr<DbSchemaEndpoint> endpoint,
    mrs::interface::AuthorizeManager *auth_manager)
    : mrs::rest::Handler(handler::get_protocol(endpoint),
                         get_endpoint_host(endpoint),
                         /* path: /service/schema/open-api-catalog */
                         /* path: /service/open-api-catalog/schema */
                         get_path_schema_openapi(endpoint),
                         get_endpoint_options(lock(endpoint)), auth_manager),
      endpoint_{endpoint} {
  auto ep = lock(endpoint_);
  entry_ = ep->get();
  auto service = lock_parent(ep);
  assert(service);
  service_entry_ = service->get();
  url_obj_ = ep->get_url().join();
}

void HandlerDbSchemaOpenAPI::authorization(rest::RequestContext *ctxt) {
  throw_unauthorize_when_check_auth_fails(ctxt);
}

HttpResult HandlerDbSchemaOpenAPI::handle_get(rest::RequestContext *ctxt) {
  rapidjson::Document json_doc;
  rapidjson::Document::AllocatorType &allocator = json_doc.GetAllocator();

  rapidjson::Value items(rapidjson::kObjectType);
  rapidjson::Value schema_properties(rapidjson::kObjectType);

  std::string full_service_path =
      get_url_host() + service_entry_->url_context_root;

  bool add_procedure_metadata{false};
  auto ep = lock(endpoint_);
  const auto &db_endpoints = ep->get_children();
  for (const auto &db_endpoint :
       rest::sort_children_by_request_path<mrs::endpoint::DbObjectEndpoint>(
           db_endpoints)) {
    auto db_object = db_endpoint->get();

    if (!rest::is_supported(db_object, entry_)) continue;

    if (db_object->requires_authentication &&
        (!authorization_manager_->is_authorized(service_entry_->id, *ctxt,
                                                &ctxt->user) ||
         (mrs::database::entry::Operation::valueRead &
          check_privileges(ctxt->user.privileges, service_entry_->id,
                           full_service_path, entry_->id, entry_->request_path,
                           db_object->id, db_object->request_path)) == 0)) {
      continue;
    }

    std::optional<uint32_t> privileges{std::nullopt};
    if (db_object->requires_authentication || entry_->requires_auth) {
      privileges =
          check_privileges(ctxt->user.privileges, service_entry_->id,
                           full_service_path, entry_->id, entry_->request_path,
                           db_object->id, db_object->request_path);
    }

    const auto is_async = mrs::rest::async_enabled(
        get_endpoint_options(std::dynamic_pointer_cast<DbObjectEndpoint>(
            db_endpoint->shared_from_this())));

    const auto path = url_obj_ + db_object->request_path;
    auto path_obj = rest::get_route_openapi_schema_path(
        privileges, db_object, path, is_async, allocator);

    for (auto path = path_obj.MemberBegin(); path < path_obj.MemberEnd();
         ++path) {
      items.AddMember(path->name, path->value, allocator);
    }

    if (is_async && (db_object->type == mrs::database::entry::DbObject::
                                            ObjectType::k_objectTypeProcedure ||
                     db_object->type == mrs::database::entry::DbObject::
                                            ObjectType::k_objectTypeFunction)) {
      const std::string task_id_path =
          url_obj_ + db_object->request_path + "/{taskId}";
      items.AddMember(
          rapidjson::Value(task_id_path, allocator),
          rest::add_task_id_endpoint(privileges, db_object, allocator),
          allocator);
    }

    if (db_object->type ==
        mrs::database::entry::DbObject::ObjectType::k_objectTypeProcedure) {
      add_procedure_metadata = true;
    }

    auto components_obj =
        rest::get_route_openapi_component(db_object, allocator);
    for (auto component = components_obj.MemberBegin();
         component < components_obj.MemberEnd(); ++component) {
      schema_properties.AddMember(component->name, component->value, allocator);
    }
  }

  if (add_procedure_metadata) {
    rest::get_procedure_metadata_component(schema_properties, allocator);
  }

  json_doc.SetObject()
      .AddMember(
          "openapi",
          rapidjson::Value(mrs::rest::k_openapi_version.data(),
                           mrs::rest::k_openapi_version.length(), allocator),
          allocator)
      .AddMember("info", rest::get_header_info(service_entry_, allocator),
                 allocator)
      .AddMember("paths", items, allocator)
      .AddMember(
          "components",
          rapidjson::Value(rapidjson::kObjectType)
              .AddMember("schemas", schema_properties, allocator)
              .AddMember("securitySchemes",
                         mrs::rest::get_security_scheme(allocator), allocator),
          allocator);

  rapidjson::StringBuffer json_buf;
  {
    rapidjson::Writer<rapidjson::StringBuffer> json_writer(json_buf);

    json_doc.Accept(json_writer);
  }

  return std::string(json_buf.GetString(), json_buf.GetLength());
}

HttpResult HandlerDbSchemaOpenAPI::handle_post(
    [[maybe_unused]] rest::RequestContext *ctxt,
    [[maybe_unused]] const std::vector<uint8_t> &document) {
  throw http::Error(HttpStatusCode::Forbidden);
}

HttpResult HandlerDbSchemaOpenAPI::handle_delete(
    [[maybe_unused]] rest::RequestContext *ctxt) {
  throw http::Error(HttpStatusCode::Forbidden);
}

HttpResult HandlerDbSchemaOpenAPI::handle_put(
    [[maybe_unused]] rest::RequestContext *ctxt) {
  throw http::Error(HttpStatusCode::Forbidden);
}

Authorization HandlerDbSchemaOpenAPI::requires_authentication() const {
  return entry_->requires_auth ? Authorization::kCheck
                               : Authorization::kNotNeeded;
}

UniversalId HandlerDbSchemaOpenAPI::get_service_id() const {
  return service_entry_->id;
}

UniversalId HandlerDbSchemaOpenAPI::get_db_object_id() const { return {}; }

UniversalId HandlerDbSchemaOpenAPI::get_schema_id() const { return entry_->id; }

const std::string &HandlerDbSchemaOpenAPI::get_service_path() const {
  return service_entry_->url_context_root;
}

const std::string &HandlerDbSchemaOpenAPI::get_db_object_path() const {
  return empty_path();
}

const std::string &HandlerDbSchemaOpenAPI::get_schema_path() const {
  return entry_->request_path;
}

uint32_t HandlerDbSchemaOpenAPI::get_access_rights() const {
  using Operation = mrs::database::entry::Operation;

  return Operation::valueRead;
}

}  // namespace handler
}  // namespace endpoint
}  // namespace mrs
