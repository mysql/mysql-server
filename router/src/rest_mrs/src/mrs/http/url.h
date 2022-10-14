/*
  Copyright (c) 2021, 2022, Oracle and/or its affiliates.

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

#include <event2/http.h>
#include <event2/keyvalq_struct.h>

#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <string_view>
#include <vector>

#include "helper/container/map.h"

#include "mysqlrouter/http_request.h"

namespace mrs {
namespace http {

class Url {
 public:
  struct Key {
    uint32_t index;
    std::string name;
  };
  using Parameaters = std::map<std::string, std::string>;
  using Keys = std::vector<std::string>;
  using Values = std::vector<std::string>;

  Url(HttpUri &uri) : uri_{uri} {
    parse_query(uri.get_query().c_str(), &parameters_);
  }

  static std::string escape_uri(const char *str) {
    auto cstr = evhttp_encode_uri(str);
    std::string result{cstr};
    free(cstr);
    return result;
  }

  static bool append_query_parameter(HttpUri &uri, const std::string &key,
                                     const std::string &value) {
    return append_query(uri, (key + "=" + escape_uri(value.c_str())));
  }

  static bool append_query(HttpUri &uri, const std::string &parameter) {
    auto q = uri.get_query();
    if (!q.empty()) q += "&";

    q += parameter;

    return uri.set_query(q);
  }

  bool remove_query_parameter(const std::string &key) {
    return 0 != parameters_.erase(key);
  }

  std::string get_query_parameter(const std::string &key) const {
    std::string result;
    helper::container::get_value(parameters_, key, &result);

    return result;
  }

  bool is_query_parameter(const std::string &key) const {
    return 0 != parameters_.count(key);
  }

  static std::string get_query_parameter(HttpUri &uri, const std::string &key) {
    Url url(uri);
    return url.get_query_parameter(key);
  }

  static void parse_query(const char *query, Keys *out_keys,
                          Values *out_values) {
    evkeyvalq key_vals;
    evhttp_parse_query_str(query, &key_vals);
    out_keys->clear();
    out_values->clear();

    auto it = key_vals.tqh_first;

    while (it) {
      out_keys->emplace_back(it->key);
      out_values->emplace_back(it->value);
      it = it->next.tqe_next;
    }

    evhttp_clear_headers(&key_vals);
  }

  static void parse_query(const char *query, Parameaters *out) {
    evkeyvalq key_vals;
    evhttp_parse_query_str(query, &key_vals);
    out->clear();

    auto it = key_vals.tqh_first;

    while (it) {
      (*out)[it->key] = it->value;
      it = it->next.tqe_next;
    }

    evhttp_clear_headers(&key_vals);
  }

  static void parse_offset_limit(const Parameaters &query, uint32_t *out_offset,
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

  static std::string extra_path_element(const std::string &base,
                                        const std::string &base_with_extra) {
    auto b_len = base.length();
    auto a_len = base_with_extra.length();

    if (a_len > b_len) {
      auto size = a_len - b_len;
      if (base_with_extra[a_len - 1] == '/') size -= 1;
      if (base_with_extra[b_len - 1] == '/' && base_with_extra[b_len] != '/') {
        return base_with_extra.substr(b_len, size);
      }

      return base_with_extra.substr(b_len + 1, size - 1);
    }

    return {};
  }

 public:
  Parameaters parameters_;
  HttpUri &uri_;
};

}  // namespace http
}  // namespace mrs

#endif  // ROUTER_SRC_REST_MRS_SRC_MRS_HTTP_URL_PATH_QUERY_H_
