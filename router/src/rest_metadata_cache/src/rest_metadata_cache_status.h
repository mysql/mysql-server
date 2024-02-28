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

#ifndef MYSQLROUTER_REST_METADATA_CACHE_STATUS_INCLUDED
#define MYSQLROUTER_REST_METADATA_CACHE_STATUS_INCLUDED

#include "mysqlrouter/rest_api_component.h"

class RestMetadataCacheStatus : public RestApiHandler {
 public:
  static constexpr const char path_regex[] = "^/metadata/([^/]+)/status/?$";

  RestMetadataCacheStatus(const std::string &require_realm)
      : RestApiHandler(require_realm, HttpMethod::Get) {}

  bool on_handle_request(http::base::Request &req, const std::string &base_path,
                         const std::vector<std::string> &path_matches) override;
};

#endif
