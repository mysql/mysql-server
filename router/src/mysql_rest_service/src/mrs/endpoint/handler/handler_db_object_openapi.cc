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
#include "mrs/endpoint/handler/handler_db_object_openapi.h"

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

auto get_path_object_openapi(std::weak_ptr<DbObjectEndpoint> endpoint) {
  using namespace std::string_literals;
  std::vector<::http::base::UriPathMatcher> result;

  auto endpoint_obj = lock(endpoint);
  if (!endpoint_obj) return result;

  auto endpoint_sch = endpoint_obj->get_parent_ptr();
  if (!endpoint_sch) return result;

  result.push_back(path_obj_openapi_swagger(endpoint_sch->get_url_path(),
                                            endpoint_obj->get()->request_path));

  return result;
}

}  // namespace

HandlerDbObjectOpenAPI::HandlerDbObjectOpenAPI(
    std::weak_ptr<DbObjectEndpoint> endpoint,
    mrs::interface::AuthorizeManager *auth_manager)
    : Handler(handler::get_protocol(endpoint), get_endpoint_host(endpoint),
              /* path: /service/schema/open-api-catalog/object */
              get_path_object_openapi(endpoint),
              get_endpoint_options(lock(endpoint)), auth_manager),
      endpoint_{endpoint} {
  auto ep = lock(endpoint_);
  auto ep_parent = lock_parent(ep);
  assert(ep_parent);
  entry_ = ep->get();
  schema_entry_ = ep_parent->get();
  auto service = lock_parent(ep_parent);
  service_entry_ = service->get();
  url_obj_ = ep->get_url().join();
}

void HandlerDbObjectOpenAPI::authorization(rest::RequestContext *ctxt) {
  throw_unauthorize_when_check_auth_fails(ctxt);
}

HttpResult HandlerDbObjectOpenAPI::handle_get(rest::RequestContext *ctxt) {
  namespace entry_ns = mrs::database::entry;

  if (entry_->enabled != entry_ns::EnabledType::EnabledType_public)
    throw http::Error(HttpStatusCode::NotFound);

  rapidjson::Document json_doc;
  rapidjson::Document::AllocatorType &allocator = json_doc.GetAllocator();

  std::string full_service_path =
      get_url_host() + service_entry_->url_context_root;

  const bool requires_auth =
      entry_->requires_authentication || schema_entry_->requires_auth;
  std::optional<uint32_t> privileges;
  if (requires_auth) {
    privileges = check_privileges(ctxt->user.privileges, service_entry_->id,
                                  full_service_path, schema_entry_->id,
                                  schema_entry_->request_path, entry_->id,
                                  entry_->request_path);
  }

  const bool is_async =
      get_options().mysql_task.driver ==
          interface::Options::MysqlTask::DriverType::kDatabase ||
      get_options().mysql_task.driver ==
          interface::Options::MysqlTask::DriverType::kRouter;
  rapidjson::Value items(rapidjson::kObjectType);
  items = rest::get_route_openapi_schema_path(privileges, entry_, url_obj_,
                                              is_async, allocator);

  if (is_async &&
      (entry_->type ==
           mrs::database::entry::DbObject::ObjectType::k_objectTypeProcedure ||
       entry_->type ==
           mrs::database::entry::DbObject::ObjectType::k_objectTypeFunction)) {
    const std::string task_id_path = url_obj_ + "/{taskId}";
    items.AddMember(rapidjson::Value(task_id_path, allocator),
                    rest::add_task_id_endpoint(privileges, entry_, allocator),
                    allocator);
  }

  auto schema_properties = rest::get_route_openapi_component(entry_, allocator);
  if (entry_->type ==
      mrs::database::entry::DbObject::ObjectType::k_objectTypeProcedure) {
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
      .AddMember("components",
                 rapidjson::Value(rapidjson::kObjectType)
                     .AddMember("schemas", schema_properties, allocator),
                 allocator);

  rapidjson::StringBuffer json_buf;
  {
    rapidjson::Writer<rapidjson::StringBuffer> json_writer(json_buf);

    json_doc.Accept(json_writer);
  }

  return std::string(json_buf.GetString(), json_buf.GetLength());
}

HttpResult HandlerDbObjectOpenAPI::handle_post(
    [[maybe_unused]] rest::RequestContext *ctxt,
    [[maybe_unused]] const std::vector<uint8_t> &document) {
  throw http::Error(HttpStatusCode::Forbidden);
}

HttpResult HandlerDbObjectOpenAPI::handle_delete(
    [[maybe_unused]] rest::RequestContext *ctxt) {
  throw http::Error(HttpStatusCode::Forbidden);
}

HttpResult HandlerDbObjectOpenAPI::handle_put(
    [[maybe_unused]] rest::RequestContext *ctxt) {
  throw http::Error(HttpStatusCode::Forbidden);
}

Authorization HandlerDbObjectOpenAPI::requires_authentication() const {
  const bool requires_auth =
      entry_->requires_authentication || schema_entry_->requires_auth;
  return requires_auth ? Authorization::kCheck : Authorization::kNotNeeded;
}

UniversalId HandlerDbObjectOpenAPI::get_service_id() const {
  return schema_entry_->service_id;
}

UniversalId HandlerDbObjectOpenAPI::get_schema_id() const {
  return schema_entry_->id;
}

UniversalId HandlerDbObjectOpenAPI::get_db_object_id() const {
  return entry_->id;
}

const std::string &HandlerDbObjectOpenAPI::get_service_path() const {
  return service_entry_->url_context_root;
}

const std::string &HandlerDbObjectOpenAPI::get_schema_path() const {
  return schema_entry_->request_path;
}

const std::string &HandlerDbObjectOpenAPI::get_db_object_path() const {
  return entry_->request_path;
}

uint32_t HandlerDbObjectOpenAPI::get_access_rights() const {
  using Operation = mrs::database::entry::Operation;

  return Operation::valueRead;
}

}  // namespace handler
}  // namespace endpoint
}  // namespace mrs
