/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_REST_MRS_SRC_MRS_HTTP_URL_PATH_QUERY_H_
#define ROUTER_SRC_REST_MRS_SRC_MRS_HTTP_URL_PATH_QUERY_H_

#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "helper/container/map.h"
#include "http/base/uri.h"

namespace helper {
namespace http {

class Url {
 public:
  struct Key {
    uint32_t index;
    std::string name;
  };
  using HttpUri = ::http::base::Uri;
  using Parameters = HttpUri::QueryElements;
  using Keys = std::vector<std::string>;
  using Values = std::vector<std::string>;

  Url(const HttpUri &uri) : uri_{uri} {}

  static void append_query_parameter(HttpUri &uri, const std::string &key,
                                     const std::string &value) {
    uri.get_query_elements()[key] = value;
  }

  std::string get_path() { return uri_.get_path(); }

  std::string get_query() { return uri_.get_query(); }

  Parameters get_query_elements() { return uri_.get_query_elements(); }

  bool remove_query_parameter(const std::string &key) {
    return 0 != uri_.get_query_elements().erase(key);
  }

  std::string get_query_parameter(const std::string &key) const {
    std::string result;
    helper::container::get_value(uri_.get_query_elements(), key, &result);

    return result;
  }

  bool get_if_query_parameter(const std::string &key,
                              std::string *value) const {
    return helper::container::get_value(uri_.get_query_elements(), key, value);
  }

  bool is_query_parameter(const std::string &key) const {
    return 0 != uri_.get_query_elements().count(key);
  }

  static std::string get_query_parameter(const HttpUri &uri,
                                         const std::string &key) {
    std::string result;
    helper::container::get_value(uri.get_query_elements(), key, &result);

    return result;
  }

  static void parse_offset_limit(const Parameters &query, uint32_t *out_offset,
                                 uint32_t *out_limit) {
    using namespace std::literals;
    if (query.empty()) return;

    auto of = query.find("offset");
    auto li = query.find("limit");

    if (of != query.end()) {
      *out_offset = atol(of->second.c_str());
    }

    if (li != query.end()) {
      *out_limit = atol(li->second.c_str());
    }
  }

  void parse_offset_limit(uint32_t *out_offset, uint32_t *out_limit) const {
    parse_offset_limit(uri_.get_query_elements(), out_offset, out_limit);
  }

 public:
  HttpUri uri_;
};

}  // namespace http
}  // namespace helper

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_HTTP_URL_PATH_QUERY_H_
