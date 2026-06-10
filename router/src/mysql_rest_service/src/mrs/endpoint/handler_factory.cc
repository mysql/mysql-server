/*
  Copyright (c) 2022, 2026, Oracle and/or its affiliates.

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

#include "mrs/endpoint/handler_factory.h"

#include "mrs/endpoint/db_service_endpoint.h"
#include "mrs/endpoint/handler/authentication/handler_authorize_auth_apps.h"
#include "mrs/endpoint/handler/authentication/handler_authorize_completed.h"
#include "mrs/endpoint/handler/authentication/handler_authorize_login.h"
#include "mrs/endpoint/handler/authentication/handler_authorize_logout.h"
#include "mrs/endpoint/handler/authentication/handler_authorize_status.h"
#include "mrs/endpoint/handler/authentication/handler_authorize_user.h"
#include "mrs/endpoint/handler/handler_content_file.h"
#include "mrs/endpoint/handler/handler_db_object_function.h"
#include "mrs/endpoint/handler/handler_db_object_metadata.h"
#include "mrs/endpoint/handler/handler_db_object_openapi.h"
#include "mrs/endpoint/handler/handler_db_object_script.h"
#include "mrs/endpoint/handler/handler_db_object_sp.h"
#include "mrs/endpoint/handler/handler_db_object_table.h"
#include "mrs/endpoint/handler/handler_db_schema_metadata.h"
#include "mrs/endpoint/handler/handler_db_schema_metadata_catalog.h"
#include "mrs/endpoint/handler/handler_db_schema_openapi.h"
#include "mrs/endpoint/handler/handler_db_service_debug.h"
#include "mrs/endpoint/handler/handler_db_service_metadata.h"
#include "mrs/endpoint/handler/handler_db_service_openapi.h"
#include "mrs/endpoint/handler/handler_redirection.h"
#include "mrs/endpoint/handler/handler_string.h"
#include "mrs/endpoint/handler/helper/utilities.h"
#include "mrs/endpoint/handler/helper/utils_proto.h"
#include "mrs/endpoint/handler/persistent/persistent_data_content_file.h"
#include "mrs/endpoint/url_host_endpoint.h"

namespace mrs {
namespace endpoint {

using namespace mrs::endpoint::handler;

using HandlerPtr = std::shared_ptr<HandlerFactory::Handler>;
using EndpointBase = mrs::interface::EndpointBase;

static std::string get_path_authnetication(
    const std::shared_ptr<mrs::database::entry::DbService> &service) {
  std::string auth_path = service->auth_path.has_value()
                              ? service->auth_path.value()
                              : "/authentication";
  return service->url_context_root + auth_path;
}

static std::string get_path_redirect(
    const std::shared_ptr<mrs::database::entry::DbService> &service) {
  if (service->auth_completed_url.has_value() &&
      !service->auth_completed_url.value().empty()) {
    return service->auth_completed_url.value();
  }

  std::string auth_path = service->auth_path.has_value()
                              ? service->auth_path.value()
                              : "/authentication";

  return service->url_context_root + auth_path + "/completed";
}

static std::shared_ptr<DbServiceEndpoint> get_endpoint_db_service(
    std::shared_ptr<EndpointBase> endpoint) {
  auto db_service_endpoint =
      std::dynamic_pointer_cast<DbServiceEndpoint>(endpoint);

  if (db_service_endpoint) return db_service_endpoint;

  auto db_schema_endpoint =
      std::dynamic_pointer_cast<DbSchemaEndpoint>(endpoint);
  if (db_schema_endpoint) {
    return lock_parent(db_schema_endpoint);
  }

  auto db_object_endpoint =
      std::dynamic_pointer_cast<DbObjectEndpoint>(endpoint);
  if (db_object_endpoint) {
    return lock_parent(lock_parent(db_object_endpoint));
  }

  auto content_set_endpoint =
      std::dynamic_pointer_cast<ContentSetEndpoint>(endpoint);
  if (content_set_endpoint) {
    return lock_parent(content_set_endpoint);
  }

  auto content_file_endpoint =
      std::dynamic_pointer_cast<ContentFileEndpoint>(endpoint);
  if (content_file_endpoint) {
    return lock_parent(lock_parent(content_file_endpoint));
  }

  return {};
}

handler::Protocol get_protocol(std::shared_ptr<EndpointBase> endpoint) {
  if (auto service_ep = get_endpoint_db_service(endpoint); service_ep.get())
    return handler::get_protocol(service_ep);

  // Endpoint is UrlHost, on this level we do not have any information
  // about the protocol. Let try to go forward with http_plugin configuration.
  return endpoint->get_configuration()->does_server_support_https()
             ? handler::k_protocolHttps
             : handler::k_protocolHttp;
}

std::string get_service_path(std::shared_ptr<EndpointBase> endpoint) {
  if (auto service_ep = get_endpoint_db_service(endpoint); service_ep.get())
    return service_ep->get()->url_context_root;

  return "";
}

class HandlerConfiguration : public mrs::rest::Handler::Configuration {
 public:
  using EndpointConfigurationPtr = HandlerFactory::EndpointConfigurationPtr;

 public:
  HandlerConfiguration(
      const HandlerFactory::EndpointConfigurationPtr &configuration)
      : configuration_{configuration} {}

  bool may_log_request() const override {
    return configuration_->get_developer().has_value();
  }

 public:
  EndpointConfigurationPtr configuration_;
};

HandlerFactory::HandlerFactory(AuthorizeManager *auth_manager,
                               GtidManager *gtid_manager,
                               MysqlCacheManager *cache_manager,
                               ResponseCache *response_cache,
                               ResponseCache *file_cache,
                               SlowQueryMonitor *slow_query_monitor,
                               MysqlTaskMonitor *task_monitor,
                               const EndpointConfigurationPtr &configuration)
    : auth_manager_{auth_manager},
      gtid_manager_{gtid_manager},
      cache_manager_{cache_manager},
      response_cache_{response_cache},
      file_cache_{file_cache},
      slow_query_monitor_(slow_query_monitor),
      task_monitor_(task_monitor),
      configuration_{configuration} {}

HandlerPtr HandlerFactory::create_db_schema_metadata_catalog_handler(
    EndpointBasePtr endpoint) {
  using namespace mrs::endpoint::handler;

  auto db_schema_endpoint =
      std::dynamic_pointer_cast<DbSchemaEndpoint>(endpoint);

  auto result = std::make_shared<HandlerDbSchemaMetadataCatalog>(
      db_schema_endpoint, auth_manager_);

  result->initialize(HandlerConfiguration{configuration_});

  return result;
}

HandlerPtr HandlerFactory::create_db_schema_openapi_handler(
    EndpointBasePtr endpoint) {
  using namespace mrs::endpoint::handler;

  auto db_schema_endpoint =
      std::dynamic_pointer_cast<DbSchemaEndpoint>(endpoint);

  auto result = std::make_shared<HandlerDbSchemaOpenAPI>(db_schema_endpoint,
                                                         auth_manager_);

  result->initialize(HandlerConfiguration{configuration_});

  return result;
}

HandlerPtr HandlerFactory::create_db_service_openapi_handler(
    EndpointBasePtr endpoint) {
  using namespace mrs::endpoint::handler;

  auto db_service_endpoint =
      std::dynamic_pointer_cast<DbServiceEndpoint>(endpoint);

  auto result = std::make_shared<HandlerDbServiceOpenAPI>(db_service_endpoint,
                                                          auth_manager_);

  result->initialize(HandlerConfiguration{configuration_});

  return result;
}

HandlerPtr HandlerFactory::create_db_object_handler(EndpointBasePtr endpoint) {
  using namespace mrs::endpoint::handler;
  using DbObjectLite = mrs::database::entry::DbObject;

  auto db_object_endpoint =
      std::dynamic_pointer_cast<DbObjectEndpoint>(endpoint);
  assert(db_object_endpoint && "Object must be castable.");

  auto entry_ = db_object_endpoint->get();
  HandlerPtr result;

  switch (entry_->type) {
    case DbObjectLite::k_objectTypeTable:
      result = std::make_shared<HandlerDbObjectTable>(
          db_object_endpoint, auth_manager_, gtid_manager_, cache_manager_,
          response_cache_, slow_query_monitor_);
      break;
    case DbObjectLite::k_objectTypeProcedure:
      result = std::make_shared<HandlerDbObjectSP>(
          db_object_endpoint, auth_manager_, gtid_manager_, cache_manager_,
          response_cache_, slow_query_monitor_, task_monitor_);
      break;
    case DbObjectLite::k_objectTypeFunction:
      result = std::make_shared<HandlerDbObjectFunction>(
          db_object_endpoint, auth_manager_, gtid_manager_, cache_manager_,
          response_cache_, slow_query_monitor_, task_monitor_);
      break;
    case DbObjectLite::k_objectTypeScript:
      result = std::make_shared<HandlerDbObjectScript>(
          db_object_endpoint, auth_manager_, gtid_manager_, cache_manager_,
          response_cache_);
      break;
  }

  if (result) result->initialize(HandlerConfiguration{configuration_});

  assert(result.get() && "all cases must be handled inside the switch.");
  return result;
}

HandlerPtr HandlerFactory::create_db_service_debug_handler(
    EndpointBasePtr endpoint) {
  auto db_service_endpoint =
      std::dynamic_pointer_cast<DbServiceEndpoint>(endpoint);
  assert(db_service_endpoint && "Object must be castable.");
  auto result = std::make_shared<HandlerDbServiceDebug>(db_service_endpoint,
                                                        auth_manager_);

  result->initialize(HandlerConfiguration{configuration_});

  return result;
}

HandlerPtr HandlerFactory::create_db_service_metadata_handler(
    EndpointBasePtr endpoint) {
  auto db_service_endpoint =
      std::dynamic_pointer_cast<DbServiceEndpoint>(endpoint);
  assert(db_service_endpoint && "Object must be castable.");

  auto result = std::make_shared<HandlerDbServiceMetadata>(db_service_endpoint,
                                                           auth_manager_);

  result->initialize(HandlerConfiguration{configuration_});

  return result;
}

HandlerPtr HandlerFactory::create_db_schema_metadata_handler(
    EndpointBasePtr endpoint) {
  auto db_schema_endpoint =
      std::dynamic_pointer_cast<DbSchemaEndpoint>(endpoint);
  assert(db_schema_endpoint && "Object must be castable.");

  auto result = std::make_shared<HandlerDbSchemaMetadata>(db_schema_endpoint,
                                                          auth_manager_);

  result->initialize(HandlerConfiguration{configuration_});

  return result;
}

HandlerPtr HandlerFactory::create_db_object_metadata_handler(
    EndpointBasePtr endpoint) {
  auto db_object_endpoint =
      std::dynamic_pointer_cast<DbObjectEndpoint>(endpoint);
  assert(db_object_endpoint && "Object must be castable.");

  auto result = std::make_shared<HandlerDbObjectMetadata>(db_object_endpoint,
                                                          auth_manager_);

  result->initialize(HandlerConfiguration{configuration_});

  return result;
}

HandlerPtr HandlerFactory::create_db_object_openapi_handler(
    EndpointBasePtr endpoint) {
  auto db_object_endpoint =
      std::dynamic_pointer_cast<DbObjectEndpoint>(endpoint);
  assert(db_object_endpoint && "Object must be castable.");

  auto result = std::make_shared<HandlerDbObjectOpenAPI>(db_object_endpoint,
                                                         auth_manager_);

  result->initialize(HandlerConfiguration{configuration_});

  return result;
}

std::shared_ptr<handler::PersistentDataContentFile>
HandlerFactory::create_persistent_content_file(
    EndpointBasePtr endpoint, const OptionalIndexNames &index_names) {
  auto content_file_endpoint =
      std::dynamic_pointer_cast<ContentFileEndpoint>(endpoint);
  assert(content_file_endpoint && "Object must be castable.");

  return make_shared<mrs::endpoint::handler::PersistentDataContentFile>(
      content_file_endpoint->get(), cache_manager_, file_cache_, index_names);
}

HandlerPtr HandlerFactory::create_content_file(
    EndpointBasePtr endpoint,
    std::shared_ptr<handler::PersistentDataContentFile> persistent_data,
    const bool handle_index) {
  auto content_file_endpoint =
      std::dynamic_pointer_cast<ContentFileEndpoint>(endpoint);
  assert(content_file_endpoint && "Object must be castable.");

  auto result = std::make_shared<HandlerContentFile>(
      content_file_endpoint, auth_manager_, persistent_data, handle_index);

  result->initialize(HandlerConfiguration{configuration_});

  return result;
}

HandlerPtr HandlerFactory::create_string_handler(
    EndpointBasePtr endpoint, const UniversalId &service_id,
    bool requires_authentication, const Uri &, const std::string &path,
    const std::string &file_name, const std::string &file_content,
    bool is_index) {
  auto protocol = get_protocol(endpoint);

  auto result = std::make_shared<HandlerString>(
      protocol, service_id, get_service_path(endpoint), requires_authentication,
      path, file_name, file_content, is_index, auth_manager_);

  result->initialize(HandlerConfiguration{configuration_});

  return result;
}

HandlerPtr HandlerFactory::create_redirection_handler(
    EndpointBasePtr endpoint, const UniversalId &service_id,
    bool requires_authentication, const Uri &url, const std::string &path,
    const std::string &file_name, const std::string &redirection_path,
    const bool pernament) {
  auto protocol = get_protocol(endpoint);

  std::string whole_path = path;
  if (!file_name.empty()) {
    whole_path += "/" + file_name;
  }
  auto result = std::make_shared<HandlerRedirection>(
      protocol, service_id, get_service_path(endpoint), requires_authentication,
      get_endpoint_host(url), whole_path, file_name, redirection_path,
      auth_manager_, pernament);

  result->initialize(HandlerConfiguration{configuration_});

  return result;
}

HandlerPtr HandlerFactory::create_authentication_login(
    EndpointBasePtr endpoint) {
  auto db_service_endpoint =
      std::dynamic_pointer_cast<DbServiceEndpoint>(endpoint);
  assert(db_service_endpoint && "Object must be castable.");
  if (!db_service_endpoint)  // in case when we add new object type at level of
    return {};               // db-service.

  auto url_host_endpoint = std::dynamic_pointer_cast<UrlHostEndpoint>(
      db_service_endpoint->get_parent_ptr());
  assert(url_host_endpoint && "UrlHost must be castable.");
  if (!url_host_endpoint)  // in case when we add new object type at level of
    return {};             // root object (url-host)

  auto entry = db_service_endpoint->get();
  auto url_host_entry = url_host_endpoint->get();

  auto url_path = ::http::base::UriPathMatcher{
      get_path_authnetication(entry) + "/login", false, false};
  auto redirect_path = get_path_redirect(entry);

  auto result = std::make_shared<HandlerAuthorizeLogin>(
      handler::get_protocol(db_service_endpoint), url_host_entry->name,
      entry->id, entry->url_context_root, url_path,
      entry->options.value_or(std::string{}), redirect_path,
      entry->auth_completed_url_validation, auth_manager_);

  result->initialize(HandlerConfiguration{configuration_});

  return result;
}

HandlerPtr HandlerFactory::create_authentication_logout(
    EndpointBasePtr endpoint) {
  auto db_service_endpoint =
      std::dynamic_pointer_cast<DbServiceEndpoint>(endpoint);
  assert(db_service_endpoint && "Object must be castable.");
  if (!db_service_endpoint)  // in case when we add new object type at level of
    return {};               // db-service.

  auto url_host_endpoint = std::dynamic_pointer_cast<UrlHostEndpoint>(
      db_service_endpoint->get_parent_ptr());
  assert(url_host_endpoint && "UrlHost must be castable.");
  if (!url_host_endpoint)  // in case when we add new object type at level of
    return {};             // root object (url-host)

  auto entry = db_service_endpoint->get();
  auto url_host_entry = url_host_endpoint->get();

  const auto url_path = ::http::base::UriPathMatcher{
      get_path_authnetication(entry) + "/logout", false, false};

  auto result = std::make_shared<HandlerAuthorizeLogout>(
      handler::get_protocol(db_service_endpoint), url_host_entry->name,
      entry->id, entry->url_context_root, url_path,
      entry->options.value_or(std::string{}), auth_manager_);

  result->initialize(HandlerConfiguration{configuration_});

  return result;
}

HandlerPtr HandlerFactory::create_authentication_completed(
    EndpointBasePtr endpoint) {
  auto db_service_endpoint =
      std::dynamic_pointer_cast<DbServiceEndpoint>(endpoint);
  assert(db_service_endpoint && "Object must be castable.");
  if (!db_service_endpoint)  // in case when we add new object type at level of
    return {};               // db-service.

  auto url_host_endpoint = std::dynamic_pointer_cast<UrlHostEndpoint>(
      db_service_endpoint->get_parent_ptr());
  assert(url_host_endpoint && "UrlHost must be castable.");
  if (!url_host_endpoint)  // in case when we add new object type at level of
    return {};             // root object (url-host)

  auto entry = db_service_endpoint->get();
  auto url_host_entry = url_host_endpoint->get();

  const auto url_path = ::http::base::UriPathMatcher{
      get_path_authnetication(entry) + "/completed", false, false};

  auto result = std::make_shared<HandlerAuthorizeCompleted>(
      handler::get_protocol(db_service_endpoint), url_host_entry->name,
      entry->id, entry->url_context_root, url_path,
      entry->options.value_or(std::string{}),
      entry->auth_completed_page_content.value_or(std::string{}),
      auth_manager_);

  result->initialize(HandlerConfiguration{configuration_});

  return result;
}

HandlerPtr HandlerFactory::create_authentication_user(
    EndpointBasePtr endpoint) {
  auto db_service_endpoint =
      std::dynamic_pointer_cast<DbServiceEndpoint>(endpoint);
  assert(db_service_endpoint && "Object must be castable.");
  if (!db_service_endpoint)  // in case when we add new object type at level of
    return {};               // db-service.

  auto url_host_endpoint = std::dynamic_pointer_cast<UrlHostEndpoint>(
      db_service_endpoint->get_parent_ptr());
  assert(url_host_endpoint && "UrlHost must be castable.");
  if (!url_host_endpoint)  // in case when we add new object type at level of
    return {};             // root object (url-host)

  auto entry = db_service_endpoint->get();
  auto url_host_entry = url_host_endpoint->get();

  auto url_path = ::http::base::UriPathMatcher{
      get_path_authnetication(entry) + "/user", false, false};

  auto result = std::make_shared<HandlerAuthorizeUser>(
      handler::get_protocol(db_service_endpoint), url_host_entry->name,
      entry->id, entry->url_context_root, url_path,
      entry->options.value_or(std::string{}), auth_manager_);

  result->initialize(HandlerConfiguration{configuration_});

  return result;
}

HandlerPtr HandlerFactory::create_authentication_auth_apps(
    EndpointBasePtr endpoint) {
  auto db_service_endpoint =
      std::dynamic_pointer_cast<DbServiceEndpoint>(endpoint);
  assert(db_service_endpoint && "Object must be castable.");
  if (!db_service_endpoint)  // in case when we add new object type at level of
    return {};               // db-service.

  auto url_host_endpoint = std::dynamic_pointer_cast<UrlHostEndpoint>(
      db_service_endpoint->get_parent_ptr());
  assert(url_host_endpoint && "UrlHost must be castable.");
  if (!url_host_endpoint)  // in case when we add new object type at level of
    return {};             // root object (url-host)

  auto entry = db_service_endpoint->get();
  auto url_host_entry = url_host_endpoint->get();

  auto url_path = ::http::base::UriPathMatcher{
      get_path_authnetication(entry) + "/authApps", false, false};
  auto redirect_path = get_path_redirect(entry);

  auto result = std::make_shared<HandlerAuthorizeAuthApps>(
      handler::get_protocol(db_service_endpoint), url_host_entry->name,
      entry->id, entry->url_context_root, url_path,
      entry->options.value_or(std::string{}), redirect_path, auth_manager_);

  result->initialize(HandlerConfiguration{configuration_});

  return result;
}

HandlerPtr HandlerFactory::create_authentication_status(
    EndpointBasePtr endpoint) {
  auto db_service_endpoint =
      std::dynamic_pointer_cast<DbServiceEndpoint>(endpoint);
  assert(db_service_endpoint && "Object must be castable.");
  if (!db_service_endpoint)  // in case when we add new object type at level of
    return {};               // db-service.

  auto url_host_endpoint = std::dynamic_pointer_cast<UrlHostEndpoint>(
      db_service_endpoint->get_parent_ptr());
  assert(url_host_endpoint && "UrlHost must be castable.");
  if (!url_host_endpoint)  // in case when we add new object type at level of
    return {};             // root object (url-host)

  auto entry = db_service_endpoint->get();
  auto url_host_entry = url_host_endpoint->get();

  auto url_path = ::http::base::UriPathMatcher{
      get_path_authnetication(entry) + "/status", false, false};

  auto result = std::make_shared<HandlerAuthorizeStatus>(
      handler::get_protocol(db_service_endpoint), url_host_entry->name,
      entry->id, entry->url_context_root, url_path,
      entry->options.value_or(std::string{}), auth_manager_);

  result->initialize(HandlerConfiguration{configuration_});

  return result;
}

}  // namespace endpoint
}  // namespace mrs
