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

#ifndef MYSQLROUTER_REST_API_INCLUDED
#define MYSQLROUTER_REST_API_INCLUDED

#include <ctime>
#include <regex>
#include <string>
#include <utility>
#include <vector>

#include "http/base/request.h"
#include "mysqlrouter/component/http_server_component.h"
#include "rest_api_plugin.h"

class RestApiHttpRequestHandler : public http::base::RequestHandler {
 public:
  RestApiHttpRequestHandler(std::shared_ptr<RestApi> rest_api)
      : rest_api_(std::move(rest_api)) {}

  void handle_request(http::base::Request &req) override;

 private:
  std::shared_ptr<RestApi> rest_api_;
};

/**
 * REST API handler for /swagger.json.
 */
class RestApiSpecHandler : public BaseRestApiHandler {
 public:
  RestApiSpecHandler(std::shared_ptr<RestApi> rest_api,
                     const std::string &require_realm)
      : rest_api_(rest_api),
        last_modified_(time(nullptr)),
        require_realm_(require_realm) {}

  bool try_handle_request(
      http::base::Request &req, const std::string &base_path,
      const std::vector<std::string> &path_matches) override;

 private:
  std::shared_ptr<RestApi> rest_api_;

  time_t last_modified_;
  std::string require_realm_;
};

#endif
