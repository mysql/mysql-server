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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_ENDPOINT_HANDLER_UTILITIES_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_ENDPOINT_HANDLER_UTILITIES_H_

#include <cassert>
#include <memory>
#include <optional>
#include <set>
#include <string>

#include "helper/json/merge.h"
#include "mrs/endpoint/content_file_endpoint.h"
#include "mrs/endpoint/content_set_endpoint.h"
#include "mrs/endpoint/db_object_endpoint.h"
#include "mrs/endpoint/db_schema_endpoint.h"
#include "mrs/endpoint/db_service_endpoint.h"
#include "mrs/http/error.h"

namespace mrs {
namespace endpoint {
namespace handler {

using Protocols = std::set<std::string>;

const uint64_t k_default_items_on_page = 25;

template <typename Type>
std::shared_ptr<Type> lock_or_throw_unavail(std::weak_ptr<Type> &endpoint) {
  auto result = endpoint.lock();

  if (!result) throw http::Error(HttpStatusCode::ServiceUnavailable);

  return result;
}

template <typename Type>
std::shared_ptr<Type> lock(const std::weak_ptr<Type> &endpoint) {
  auto result = endpoint.lock();

  assert(result &&
         "The weak_ptr should be not expired, when calling any Handler "
         "constructor.");

  return result;
}

template <typename Type>
const std::shared_ptr<Type> &lock(std::shared_ptr<Type> &endpoint) {
  // Nothing to lock
  return endpoint;
}

inline std::string get_endpoint_host(const ::http::base::Uri &url) {
  auto result = url.get_host();
  if (!result.empty()) {
    auto port = url.get_port();
    if (-1 != port) {
      return result + ":" + std::to_string(port);
    }
  }
  return result;
}

inline std::string get_endpoint_host(
    std::weak_ptr<mrs::interface::EndpointBase> wp) {
  auto endpoint = lock(wp);
  if (!endpoint) return {};

  return get_endpoint_host(endpoint->get_url());
}

inline std::shared_ptr<DbSchemaEndpoint> lock_parent(
    const std::shared_ptr<DbObjectEndpoint> &endpoint) {
  auto parent = endpoint->get_parent_ptr();
  if (!parent) return {};

  return std::dynamic_pointer_cast<DbSchemaEndpoint>(parent);
}

inline std::shared_ptr<DbServiceEndpoint> lock_parent(
    const std::shared_ptr<DbSchemaEndpoint> &endpoint) {
  auto parent = endpoint->get_parent_ptr();
  if (!parent) return {};

  return std::dynamic_pointer_cast<DbServiceEndpoint>(parent);
}

inline std::shared_ptr<ContentSetEndpoint> lock_parent(
    ContentFileEndpoint *endpoint) {
  auto parent = endpoint->get_parent_ptr();
  if (!parent) return {};

  return std::dynamic_pointer_cast<ContentSetEndpoint>(parent);
}

inline std::shared_ptr<ContentSetEndpoint> lock_parent(
    const std::shared_ptr<ContentFileEndpoint> &endpoint) {
  auto parent = endpoint->get_parent_ptr();
  if (!parent) return {};

  return std::dynamic_pointer_cast<ContentSetEndpoint>(parent);
}

inline std::shared_ptr<DbServiceEndpoint> lock_parent(
    const std::shared_ptr<ContentSetEndpoint> &endpoint) {
  auto parent = endpoint->get_parent_ptr();
  if (!parent) return {};

  return std::dynamic_pointer_cast<DbServiceEndpoint>(parent);
}

inline std::optional<std::string> merge_options(
    std::optional<std::string> options,
    std::optional<std::string> parent_options) {
  static const std::set<std::string> k_non_inherited_options{
      "directoryIndexDirective", "defaultStaticContent", "defaultRedirects"};
  static const std::set<std::string> k_always_inherited_options{
      "passthroughDbUser"};
  return helper::json::merge_objects(options, parent_options,
                                     k_non_inherited_options,
                                     k_always_inherited_options);
}

inline std::optional<std::string> get_endpoint_options(
    const std::shared_ptr<DbServiceEndpoint> &endpoint) {
  return endpoint->get()->options;
}

inline std::optional<std::string> get_endpoint_options(
    const std::shared_ptr<DbSchemaEndpoint> &endpoint) {
  const auto &option = endpoint->get()->options;

  auto parent =
      std::dynamic_pointer_cast<DbServiceEndpoint>(endpoint->get_parent_ptr());
  if (!parent) return option;

  const auto &parent_options = get_endpoint_options(parent);
  return merge_options(option, parent_options);
}

inline std::optional<std::string> get_endpoint_options(
    const std::shared_ptr<DbObjectEndpoint> &endpoint) {
  const auto &option = endpoint->get()->options;

  auto parent =
      std::dynamic_pointer_cast<DbSchemaEndpoint>(endpoint->get_parent_ptr());
  if (!parent) return option;

  const auto &parent_options = get_endpoint_options(parent);
  return merge_options(option, parent_options);
}

inline std::optional<std::string> get_endpoint_options(
    const std::shared_ptr<ContentSetEndpoint> &endpoint) {
  const auto &option = endpoint->get()->options;

  auto parent =
      std::dynamic_pointer_cast<DbServiceEndpoint>(endpoint->get_parent_ptr());
  if (!parent) return option;

  const auto &parent_options = get_endpoint_options(parent);
  return merge_options(option, parent_options);
}

inline std::optional<std::string> get_endpoint_options(
    const std::shared_ptr<ContentFileEndpoint> &endpoint) {
  const auto &option = endpoint->get()->options;

  auto parent =
      std::dynamic_pointer_cast<ContentSetEndpoint>(endpoint->get_parent_ptr());
  if (!parent) return option;

  const auto &parent_options = get_endpoint_options(parent);
  return merge_options(option, parent_options);
}

inline Protocols get_endpoint_protocol(
    std::shared_ptr<DbServiceEndpoint> &endpoint) {
  auto entry = endpoint->get();

  return entry->url_protocols;
}

template <typename Endpoint>
Protocols get_endpoint_protocol(std::shared_ptr<Endpoint> &endpoint) {
  auto parent = lock_parent(endpoint);
  if (!parent) return {};

  return get_endpoint_protocol(parent);
}

}  // namespace handler
}  // namespace endpoint
}  // namespace mrs

#endif /* ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_ENDPOINT_HANDLER_UTILITIES_H_ \
        */
