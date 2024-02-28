/*
  Copyright (c) 2019, 2024, Oracle and/or its affiliates.

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

#ifndef MYSQLROUTER_REST_API_PLUGIN_INCLUDED
#define MYSQLROUTER_REST_API_PLUGIN_INCLUDED

#include "mysqlrouter/rest_api_component.h"

#include <list>
#include <regex>
#include <shared_mutex>

class RestApi {
 public:
  RestApi(const std::string &uri_prefix, const std::string &uri_prefix_regex);

  RestApi(const RestApi &) = delete;
  RestApi &operator=(const RestApi &) = delete;

  RestApi(RestApi &&) = delete;
  RestApi &operator=(RestApi &&) = delete;

  /**
   * process the spec's Json JsonDocument.
   */
  void process_spec(RestApiComponent::SpecProcessor spec_processor);

  using PathList =
      std::list<std::tuple<std::regex, std::unique_ptr<BaseRestApiHandler>>>;

  /**
   * add handler for URI path.
   */
  void add_path(const std::string &path,
                std::unique_ptr<BaseRestApiHandler> handler);

  /**
   * remove handle for URI path.
   */
  void remove_path(const std::string &path);

  /**
   * handle request for all register URI paths.
   *
   * if no handler accepts the request, a HTTP response with status 404 will be
   * sent.
   */
  void handle_paths(http::base::Request &req);

  /**
   * get the uri path prefix.
   */
  std::string uri_prefix() const { return uri_prefix_; }

  /**
   * get the regex for the URI path prefix.
   */
  std::string uri_prefix_regex() const { return uri_prefix_regex_; }

  /**
   * get the spec as JSON.
   */
  std::string spec();

 protected:
  std::string uri_prefix_;
  std::string uri_prefix_regex_;

  std::shared_timed_mutex rest_api_handler_mutex_;
  std::list<
      std::tuple<std::string, std::regex, std::unique_ptr<BaseRestApiHandler>>>
      rest_api_handlers_;

  std::mutex spec_doc_mutex_;
  RestApiComponent::JsonDocument spec_doc_;
};

#endif
