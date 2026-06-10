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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_ENDPOINT_HANDLER_URL_PATHS_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_ENDPOINT_HANDLER_URL_PATHS_H_

#include <string>
#include <vector>

#include "helper/string/contains.h"
#include "http/base/uri.h"
#include "http/base/uri_path_matcher.h"

namespace mrs {
namespace endpoint {
namespace handler {

const std::string k_openapi_catalog = "/open-api-catalog";
const std::string k_path_metadata_catalog = "metadata-catalog";
const std::string k_metadata = "_metadata";
const std::string k_debug = "_debug";

inline auto path_schema_catalog(const std::string &service_schema_path) {
  using namespace std::string_literals;

  return ::http::base::UriPathMatcher{
      service_schema_path + "/"s + k_path_metadata_catalog, false, true};
}

inline std::string url_sch_metadata_catalog(const ::http::base::Uri &uri) {
  auto u = uri;
  u.get_path_elements().push_back(k_path_metadata_catalog);
  return u.join();
}

inline auto path_service_debug(const std::string &service_path) {
  using namespace std::string_literals;

  return ::http::base::UriPathMatcher{service_path + "/"s + k_debug, false,
                                      true};
}

inline auto path_service_metadata(const std::string &service_path) {
  using namespace std::string_literals;

  return ::http::base::UriPathMatcher{service_path + "/"s + k_metadata, false,
                                      true};
}

inline auto path_schema_metadata(const std::string &service_schema_path) {
  using namespace std::string_literals;

  return ::http::base::UriPathMatcher{service_schema_path + "/"s + k_metadata,
                                      false, true};
}

inline auto path_object_metadata(const std::string &service_schema_path,
                                 const std::string &obj_name) {
  using namespace std::string_literals;

  return ::http::base::UriPathMatcher{
      service_schema_path + obj_name + "/"s + k_metadata, false, true};
}

inline auto path_obj_openapi_swagger(const std::string &service_schema_path,
                                     const std::string &obj_name) {
  using namespace std::string_literals;

  return ::http::base::UriPathMatcher{
      service_schema_path + k_openapi_catalog + obj_name, false, true};
}

inline auto path_schema_openapi_swagger(
    const std::string &service_schema_path) {
  using namespace std::string_literals;

  return ::http::base::UriPathMatcher{service_schema_path + k_openapi_catalog,
                                      false, true};
}

inline auto path_schema_openapi_swagger_alias(const std::string &service_name,
                                              const std::string &schema_name) {
  using namespace std::string_literals;

  return ::http::base::UriPathMatcher{
      "/"s + service_name + k_openapi_catalog + "/" + schema_name, false, true};
}

inline auto path_service_openapi_swagger(const std::string &service_path) {
  using namespace std::string_literals;

  return ::http::base::UriPathMatcher{service_path + k_openapi_catalog, false,
                                      true};
}

inline auto path_db_object_with_index(const std::string &object_path,
                                      const std::string &service_schema_path,
                                      const bool is_index) {
  using namespace std::string_literals;
  std::vector<::http::base::UriPathMatcher> result{{object_path, true, false}};

  if (is_index) {
    // When the url path is empty, its root path, which
    // http plugin processes as "", instead "/".
    if (service_schema_path.empty())
      result.emplace_back(service_schema_path, false, false);
    else
      result.emplace_back(service_schema_path + "/", false, false);
  }

  return result;
}

inline auto path_file(std::string service_schema_path,
                      const std::string &object_path, bool is_index) {
  using namespace std::string_literals;
  std::vector<::http::base::UriPathMatcher> result{
      {service_schema_path + object_path, false, false}};

  if (is_index) {
    // When the url path is empty, it's root path, which
    // http plugin processes as "", instead "/".
    if (service_schema_path.empty())
      result.emplace_back(service_schema_path, false, false);
    else
      result.emplace_back(service_schema_path + "/", false, false);
  }

  return result;
}

inline std::string remove_leading_slash_from_path(const std::string &path) {
  if (path.empty()) return {};
  if (path[0] == '/') return path.substr(1);
  return {};
}

inline auto path_content_file(const std::string &service_schema_path) {
  return path_file(service_schema_path, "",
                   helper::ends_with(service_schema_path, "/index.html"));
}

}  // namespace handler
}  // namespace endpoint
}  // namespace mrs

#endif /* ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_ENDPOINT_HANDLER_URL_PATHS_H_ \
        */
