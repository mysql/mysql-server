/*
  Copyright (c) 2019, 2023, Oracle and/or its affiliates.

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

#ifndef MYSQLROUTER_REST_API_COMPONENT_INCLUDED
#define MYSQLROUTER_REST_API_COMPONENT_INCLUDED

#include <mutex>

#include "mysqlrouter/rest_api_export.h"

#ifdef RAPIDJSON_NO_SIZETYPEDEFINE
#include "my_rapidjson_size_t.h"
#endif

#include <rapidjson/document.h>
#include <rapidjson/pointer.h>
#include <rapidjson/prettywriter.h>
#include <rapidjson/schema.h>

#include "mysqlrouter/rest_client.h"

class RestApi;

class REST_API_EXPORT BaseRestApiHandler {
 public:
  BaseRestApiHandler() = default;
  BaseRestApiHandler(const BaseRestApiHandler &) = default;
  BaseRestApiHandler(BaseRestApiHandler &&) = default;
  BaseRestApiHandler &operator=(const BaseRestApiHandler &) = default;
  BaseRestApiHandler &operator=(BaseRestApiHandler &&) = default;
  /**
   * try to handle the request.
   *
   * @returns success
   * @retval true request has been handled and a response has been sent
   * @retval false request has not been handled (no response has been sent)
   */
  virtual bool try_handle_request(
      HttpRequest &req, const std::string &base_path,
      const std::vector<std::string> &path_matches) = 0;

  virtual ~BaseRestApiHandler();
};

/**
 * handler for REST API calls.
 *
 * - may require authentication
 * - enforces HTTP Methods
 */
class REST_API_EXPORT RestApiHandler : public BaseRestApiHandler {
 public:
  RestApiHandler(const std::string &require_realm,
                 HttpMethod::Bitset allowed_methods)
      : require_realm_(require_realm), allowed_methods_(allowed_methods) {}

  bool try_handle_request(
      HttpRequest &req, const std::string &base_path,
      const std::vector<std::string> &path_matches) override;

  virtual bool on_handle_request(
      HttpRequest &req, const std::string &base_path,
      const std::vector<std::string> &path_matches) = 0;

 private:
  std::string require_realm_;

  HttpMethod::Bitset allowed_methods_;
};

class REST_API_EXPORT RestApiComponent {
 public:
  // AddressSanitizer gets confused by the default, MemoryPoolAllocator
  // Solaris sparc also gets crashes
  using JsonDocument =
      rapidjson::GenericDocument<rapidjson::UTF8<>, rapidjson::CrtAllocator>;
  using JsonValue =
      rapidjson::GenericValue<rapidjson::UTF8<>, JsonDocument::AllocatorType>;
  using JsonPointer =
      rapidjson::GenericPointer<JsonValue, JsonDocument::AllocatorType>;

  /**
   * get singleton instance of the component.
   */
  static RestApiComponent &get_instance();

  /**
   * initialize component.
   *
   * registers RestApi with the component and actives the processing of
   * the backlogs for:
   *
   * - try_process_spec()
   * - add_path()
   */
  void init(std::shared_ptr<RestApi> srv);

  /**
   * processor for the RestAPI's spec.
   *
   * @param spec_doc JSON document to modify
   */
  using SpecProcessor = void (*)(JsonDocument &spec_doc);

  /**
   * try to process the RestAPI's spec.
   *
   * if the component hasn't been initialized from the rest_api plugin yet,
   * false is returned and the processor is added to a backlog which is
   * processed when init() is called.
   *
   * As the rest_api may fail to load, the caller should remove itself again
   * with remove_process_spec() in that case.
   *
   * That's not needed in case try_process_spec() returns true.
   *
   * @param processor document processor
   * @returns success
   * @retval true spec was processed.
   * @retval false processor added to backlog.
   */
  bool try_process_spec(SpecProcessor processor);

  /**
   * remove processor from backlog if exists.
   *
   * @param processor document processor
   */
  void remove_process_spec(SpecProcessor processor);

  /**
   * added handler for a path.
   *
   * path must be unique
   *
   * @param path regex for the path
   * @param handler handler for the path
   */
  void add_path(const std::string &path,
                std::unique_ptr<BaseRestApiHandler> handler);

  /**
   * remove a path.
   *
   * must be called before the plugin gets unloaded that added the handler
   * in the first place.
   */
  void remove_path(const std::string &path);

 private:
  // disable copy, as we are a single-instance
  RestApiComponent(RestApiComponent const &) = delete;
  void operator=(RestApiComponent const &) = delete;

  std::mutex spec_mu_;  // backlog mutex mutex
  std::vector<SpecProcessor> spec_processors_;
  std::vector<std::pair<std::string, std::unique_ptr<BaseRestApiHandler>>>
      add_path_backlog_;

  std::weak_ptr<RestApi> srv_;

  RestApiComponent() = default;
};

/**
 * Helper class to make unregistering paths in plugins easier.
 */
class RestApiComponentPath {
 public:
  RestApiComponentPath(RestApiComponent &rest_api_srv, std::string regex,
                       std::unique_ptr<BaseRestApiHandler> endpoint)
      : rest_api_srv_{rest_api_srv}, regex_(std::move(regex)) {
    rest_api_srv_.add_path(regex_, std::move(endpoint));
  }

  ~RestApiComponentPath() {
    try {
      rest_api_srv_.remove_path(regex_);
    } catch (...) {
      // if it already is removed manually, ignore it
    }
  }

 private:
  RestApiComponent &rest_api_srv_;
  std::string regex_;
};

#endif
