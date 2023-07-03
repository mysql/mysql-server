/*
  Copyright (c) 2019, 2022, Oracle and/or its affiliates.

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

#include "rest_api.h"

#include "mysqlrouter/rest_api_utils.h"

bool RestApiSpecHandler::try_handle_request(HttpRequest &req,
                                            const std::string & /* base_path */,
                                            const std::vector<std::string> &) {
  if (!ensure_http_method(req, HttpMethod::Get | HttpMethod::Head)) {
    return true;
  }

  if (!ensure_auth(req, require_realm_)) {
    return true;
  }

  if (!ensure_no_params(req)) return true;

  auto out_hdrs = req.get_output_headers();
  out_hdrs.add("Content-Type", "application/json");

  if (!req.is_modified_since(last_modified_)) {
    req.send_reply(HttpStatusCode::NotModified);
    return true;
  }

  const std::string spec = rest_api_->spec();

  req.add_last_modified(last_modified_);
  if (req.get_method() == HttpMethod::Get) {
    auto chunk = req.get_output_buffer();
    chunk.add(spec.data(), spec.size());

    req.send_reply(HttpStatusCode::Ok, "Ok", chunk);
  } else {
    // HEAD has no content, but a Content-Length
    //
    // instead of sending a response and let evhttp_ discard
    // the header, we set Content-Length directly
    out_hdrs.add("Content-Length", std::to_string(spec.size()).c_str());
    req.send_reply(HttpStatusCode::Ok);
  }

  return true;
}

void RestApiHttpRequestHandler::handle_request(HttpRequest &req) {
  rest_api_->handle_paths(req);
}
