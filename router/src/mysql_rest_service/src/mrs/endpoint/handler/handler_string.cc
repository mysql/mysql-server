/*
 Copyright (c) 2021, 2026, Oracle and/or its affiliates.

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

#include "mrs/endpoint/handler/handler_string.h"

#include <map>
#include <string>
#include <vector>

#include "mysql/harness/logging/logging.h"
#include "mysql/harness/string_utils.h"

#include "helper/media_type.h"
#include "mrs/endpoint/handler/helper/url_paths.h"
#include "mrs/http/error.h"
#include "mrs/rest/request_context.h"
#include "mysqlrouter/base64.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace endpoint {
namespace handler {

using namespace std::string_literals;

template <typename T>
static std::string as_string(const std::vector<T> &v) {
  return std::string(v.begin(), v.end());
}
const std::string k_slash{"/"};

HandlerString::HandlerString(
    const Protocol protocol, const UniversalId &service_id,
    const std::string &service_path, bool requires_authentication,
    const std::string &path, const std::string &file_name,
    const std::string &file_content, const bool is_index,
    mrs::interface::AuthorizeManager *auth_manager)
    : Handler(protocol, "", path_file(path, k_slash + file_name, is_index),
              std::string{}, auth_manager),
      service_id_{service_id},
      service_path_{service_path},
      requires_authentication_{requires_authentication},
      path_{path + mysql_harness::Path::directory_separator + file_name},
      schema_path_{path},
      file_name_{file_name},
      file_content_{file_content} {
  mysql_harness::Path p{mysql_harness::Path::directory_separator + file_name_};
  type_ = helper::get_media_type_from_extension(
      mysql_harness::make_lower(p.extension()).c_str());

  if (!helper::is_text_type(type_)) {
    try {
      file_content_ = as_string(Base64::decode(file_content_));
    } catch (const std::exception &e) {
      log_debug("HandlerString - file:%s, content decoding failed with %s",
                file_name_.c_str(), e.what());
      // keep old file_content_
    }
  }
}

UniversalId HandlerString::get_service_id() const { return service_id_; }

UniversalId HandlerString::get_schema_id() const { return {}; }

UniversalId HandlerString::get_db_object_id() const { return {}; }

const std::string &HandlerString::get_service_path() const {
  return service_path_;
}

const std::string &HandlerString::get_schema_path() const {
  return schema_path_;
}

const std::string &HandlerString::get_db_object_path() const { return path_; }

HandlerString::Authorization HandlerString::requires_authentication() const {
  return requires_authentication_ ? Authorization::kCheck
                                  : Authorization::kNotNeeded;
}

uint32_t HandlerString::get_access_rights() const {
  return mrs::database::entry::Operation::valueRead;
}

void HandlerString::authorization(rest::RequestContext *) {}

HandlerString::HttpResult HandlerString::handle_get(rest::RequestContext *) {
  return {file_content_, type_};
}

HandlerString::HttpResult HandlerString::handle_delete(rest::RequestContext *) {
  throw http::Error(HttpStatusCode::NotImplemented);
}

HandlerString::HttpResult HandlerString::handle_post(
    rest::RequestContext *, const std::vector<uint8_t> &) {
  throw http::Error(HttpStatusCode::NotImplemented);
}

HandlerString::HttpResult HandlerString::handle_put(rest::RequestContext *) {
  throw http::Error(HttpStatusCode::NotImplemented);
}

}  // namespace handler
}  // namespace endpoint
}  // namespace mrs
