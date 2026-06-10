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

#include "mrs/endpoint/handler/handler_debug.h"

#include <string>
#include <vector>

#include "mrs/rest/request_context.h"

#include "mysql/harness/logging/logging.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace endpoint {
namespace handler {

using namespace std::string_literals;

const ::http::base::UriPathMatcher g_matcher{"/debug", false, false};

HandlerDebug::HandlerDebug(HandlerCallback *cb)
    : Handler(::mrs::endpoint::handler::Protocol::k_protocolHttp, "",
              {g_matcher}, "", nullptr),
      cb_{cb} {}

const std::string &HandlerDebug::get_service_path() const {
  return empty_path();
}

const std::string &HandlerDebug::get_schema_path() const {
  return empty_path();
}

const std::string &HandlerDebug::get_db_object_path() const {
  return empty_path();
}

HandlerDebug::HttpResult HandlerDebug::handle_get(rest::RequestContext *ctxt) {
  auto query = ctxt->get_http_url().get_query_elements();
  if (query.count("do")) {
    auto value = query["do"];
    if (value == "start") {
      cb_->handler_start();
      return HttpResult{HttpStatusCode::Ok, "{}", helper::MediaType::typeJson};
    } else if (value == "stop") {
      cb_->handler_stop();
      return HttpResult{HttpStatusCode::Ok, "{}", helper::MediaType::typeJson};
    }
  }
  return HttpResult{HttpStatusCode::BadRequest,
                    "{\"message\":\"Missing or invalid 'do' query parameter\"}",
                    helper::MediaType::typeJson};
}

}  // namespace handler
}  // namespace endpoint
}  // namespace mrs
