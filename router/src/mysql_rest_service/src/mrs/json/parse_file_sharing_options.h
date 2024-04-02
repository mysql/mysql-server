/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#ifndef ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_JSON_PARSE_FILE_SHARING_OPTIONS_H_
#define ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_JSON_PARSE_FILE_SHARING_OPTIONS_H_

#include <map>
#include <string>
#include <vector>

#include "mysqlrouter/base64.h"

#include "helper/json/rapid_json_to_struct.h"
#include "helper/string/contains.h"

namespace mrs {
namespace json {

class FileSharing {
 public:
  std::map<std::string, std::string> default_static_content_;
  std::map<std::string, std::string> default_redirects_;
  std::vector<std::string> directory_index_directive_;
};

class ParseFileSharingOptions
    : public helper::json::RapidReaderHandlerToStruct<FileSharing> {
 private:
  template <typename T>
  std::string to_string(const T &v) {
    return std::to_string(v);
  }
  std::string to_string(const std::string &str) {
    try {
      return from_base64(str);
    } catch (...) {
    }
    return str;
  }

  template <typename T>
  std::string from_base64(const T &str) {
    return to_string(str);
  }

  std::string from_base64(const std::string &str) {
    return helper::as_string(Base64::decode(str));
  }

  template <typename Container>
  void push_key_value(Container *c, const std::string &,
                      const std::string &value) {
    c->push_back(value);
  }

  void push_key_value(std::map<std::string, std::string> *c,
                      const std::string &key, const std::string &value) {
    (*c)[key] = value;
  }

 public:
  template <typename ValueType, typename Container>
  bool push_value(const std::string &starts, const std::string &key,
                  const ValueType &vt, Container *push_to) {
    if (helper::starts_with(key, starts)) {
      push_key_value(push_to, key.substr(starts.length()), to_string(vt));
      return true;
    }
    return false;
  }

  template <typename ValueType>
  void handle_array_value(const std::string &key, const ValueType &vt) {
    static const std::string kHttpContent = "directoryIndexDirective.";
    push_value(kHttpContent, key, vt, &result_.directory_index_directive_);
  }

  template <typename ValueType>
  void handle_object_value(const std::string &key, const ValueType &vt) {
    static const std::string kHttpContent = "defaultStaticContent.";
    static const std::string kHttpRedirects = "defaultRedirects.";
    using std::to_string;

    if (!push_value(kHttpContent, key, vt, &result_.default_static_content_)) {
      push_value(kHttpRedirects, key, vt, &result_.default_redirects_);
    }
  }

  template <typename ValueType>
  void handle_value(const ValueType &vt) {
    const auto &key = get_current_key();
    if (is_object_path()) {
      handle_object_value(key, vt);
    } else if (is_array_value()) {
      handle_array_value(key, vt);
    }
  }

  bool String(const Ch *v, rapidjson::SizeType v_len, bool) override {
    handle_value(std::string{v, v_len});
    return true;
  }

  bool RawNumber(const Ch *v, rapidjson::SizeType v_len, bool) override {
    handle_value(std::string{v, v_len});
    return true;
  }

  bool Bool(bool v) override {
    handle_value(v);
    return true;
  }
};

}  // namespace json
}  // namespace mrs

#endif  // ROUTER_SRC_MYSQL_REST_SERVICE_SRC_MRS_JSON_PARSE_FILE_SHARING_OPTIONS_H_
