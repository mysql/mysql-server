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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_REST_REST_OPENAPI_OBJECT_CREATOR_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_REST_REST_OPENAPI_OBJECT_CREATOR_H_

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>

#include <optional>
#include <string_view>

#include "mrs/database/entry/db_object.h"
#include "mrs/database/entry/db_schema.h"
#include "mrs/database/entry/db_service.h"
#include "mrs/endpoint/db_object_endpoint.h"
#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace rest {

using DbObject = mrs::database::entry::DbObject;
using DbService = mrs::database::entry::DbService;
using DbObjectPtr = std::shared_ptr<DbObject>;

constexpr std::string_view k_auth_method_name = "mrs_login";
constexpr std::string_view k_schema_version{"1.0.0"};
constexpr std::string_view k_openapi_version{"3.1.0"};

/**
 * Create OpenAPI 'paths' section. Each path contains supported HTTP methods
 * with HTTP responses and optional parameters.
 *
 * @param[in] privileges User privileges
 * @param[in] entry DBobject entry
 * @param[in] url path used by DBobject entry
 * @param[in] is_async BDobject supports async operations
 * @param[in] allocator JSON allocator that is used to create OpenAPI swagger.
 *
 * @return OpenAPI paths JSON.
 */
rapidjson::Value get_route_openapi_schema_path(
    const std::optional<uint32_t> privileges, DbObjectPtr entry,
    const std::string &url, const bool is_async,
    rapidjson::Document::AllocatorType &allocator);

/**
 * Create OpenAPI components section containing security schemes and schemas
 * (type information with constraints) for each db object passed as a parameter.
 *
 * @param[in] entry DBobject entry
 * @param[in] allocator JSON allocator that is used to create OpenAPI swagger.
 *
 * @return OpenAPI components JSON.
 */
rapidjson::Value get_route_openapi_component(
    DbObjectPtr entry, rapidjson::Document::AllocatorType &allocator);

/**
 * Create "_metadata" schema component item from a procedure call.
 *
 * @param[in, out] schema_properties JSON containing schema components.
 * @param[in] allocator JSON allocator that is used to create OpenAPI swagger.
 */
void get_procedure_metadata_component(
    rapidjson::Value &schema_properties,
    rapidjson::Document::AllocatorType &allocator);

/**
 * Create OpenAPI title, version and description.
 *
 * @param[in] service DBservice entry.
 * @param[in] allocator JSON allocator that is used to create OpenAPI swagger.
 *
 * @return OpenAPI header JSON.
 */
rapidjson::Value get_header_info(std::shared_ptr<DbService> service,
                                 rapidjson::Document::AllocatorType &allocator);

/**
 * Check if the given DB Object entry can be used for getting an OpenAPI
 * description. It must be enabled, its schema must be enabled, and it have to
 * be in appropriate type.
 *
 * @param[in] db_obj DB Object entry
 * @param[in] db_schema DB Schema entry
 *
 * @retval true DB Object entry might be used.
 * @retval false DB Object entry might not be used.
 */
bool is_supported(
    const std::shared_ptr<mrs::database::entry::DbObject> &db_obj,
    const std::shared_ptr<mrs::database::entry::DbSchema> &db_schema);

/**
 * Create security scheme for OpenAPI.
 *
 * @param[in] allocator JSON allocator that is used to create OpenAPI swagger.
 *
 * @return OpenAPI security scheme object.
 */
rapidjson::Value get_security_scheme(
    rapidjson::Document::AllocatorType &allocator);

/**
 * Sort Endpoint children by request path.
 *
 * @param[in] children Endpoint children.
 * @tparam R returned Endpoint type.
 *
 * @return sorted endpoints.
 */
template <typename R>
std::vector<R *> sort_children_by_request_path(
    std::vector<std::shared_ptr<mrs::interface::EndpointBase>> children) {
  std::vector<R *> result;
  for (const auto &child : children) {
    auto child_endpoint = std::dynamic_pointer_cast<R>(child);
    if (!child_endpoint) continue;

    result.push_back(child_endpoint.get());
  }

  std::sort(std::begin(result), std::end(result),
            [](const auto &a, const auto &b) {
              return a->get()->request_path < b->get()->request_path;
            });

  return result;
}

/**
 * Check if asynchronous tasks are enabled in options.
 *
 * @param[in] options Options configured for the endpoint
 *
 * @return information if async operations are enabled
 */
bool async_enabled(const std::optional<std::string> &options);

/**
 * Add specification related to endpoint responsible for async operations
 * (located at /service/schema/object/{taskId}).
 *
 * @param[in] privileges User privileges
 * @param[in] entry DBobject entry
 * @param[in] allocator JSON allocator that is used to create OpenAPI swagger.
 *
 * @return OpenAPI paths JSON.
 */
rapidjson::Value add_task_id_endpoint(
    const std::optional<uint32_t> privileges, DbObjectPtr entry,
    rapidjson::Document::AllocatorType &allocator);

}  // namespace rest
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_REST_REST_OPENAPI_OBJECT_CREATOR_H_
