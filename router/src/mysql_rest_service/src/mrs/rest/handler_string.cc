/*
 Copyright (c) 2021, 2023, Oracle and/or its affiliates.

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

#include "mrs/rest/handler_string.h"

#include <map>
#include <string>
#include <vector>

#include "mysql/harness/logging/logging.h"
#include "mysql/harness/string_utils.h"
#include "mysqlrouter/http_request.h"

#include "mrs/database/query_entry_content_file.h"
#include "mrs/database/query_factory.h"
#include "mrs/http/error.h"
#include "mrs/rest/request_context.h"

IMPORT_LOG_FUNCTIONS()

using Type = mrs::interface::RestHandler::HttpResult::Type;

static Type get_result_type_from_extension(const std::string &ext) {
  static std::map<std::string, Type> map{
      {".gif", Type::typeGif},  {".jpg", Type::typeJpg},
      {".png", Type::typePng},  {".js", Type::typeJs},
      {".mjs", Type::typeJs},   {".html", Type::typeHtml},
      {".htm", Type::typeHtml}, {".css", Type::typeCss},
      {".svg", Type::typeSvg},  {".map", Type::typePlain},
      {".ico", Type::typeIco}};

  log_debug("ext:'%s'", ext.c_str());
  auto i = map.find(ext);

  if (i == map.end()) return Type::typeHtml;

  return i->second;
}

namespace mrs {
namespace rest {

using namespace std::string_literals;

using HttpResult = Handler::HttpResult;

HandlerString::HandlerString(const std::string &path,
                             const std::string &content,
                             mrs::interface::AuthorizeManager *auth_manager)
    : Handler("url-not-set", {"^"s + path + "$"}, {}, auth_manager),
      path_{path},
      content_{content} {}

UniversalId HandlerString::get_service_id() const { return {}; }
UniversalId HandlerString::get_db_object_id() const { return {}; }
UniversalId HandlerString::get_schema_id() const { return {}; }

Handler::Authorization HandlerString::requires_authentication() const {
  return Authorization::kNotNeeded;
}

uint32_t HandlerString::get_access_rights() const {
  return mrs::interface::Object::kRead;
}

void HandlerString::authorization(rest::RequestContext *) {}

HttpResult HandlerString::handle_get(rest::RequestContext *) {
  mysql_harness::Path path{path_};
  return {content_, get_result_type_from_extension(
                        mysql_harness::make_lower(path.extension()))};
}

HttpResult HandlerString::handle_delete(rest::RequestContext *) {
  throw http::Error(HttpStatusCode::NotImplemented);
}

HttpResult HandlerString::handle_post(rest::RequestContext *,
                                      const std::vector<uint8_t> &) {
  throw http::Error(HttpStatusCode::NotImplemented);
}

HttpResult HandlerString::handle_put(rest::RequestContext *) {
  throw http::Error(HttpStatusCode::NotImplemented);
}

}  // namespace rest
}  // namespace mrs
