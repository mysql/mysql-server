/*
  Copyright (c) 2018, Oracle and/or its affiliates. All rights reserved.

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

#include "mysqlrouter/rest_client.h"

HttpRequest RestClient::request_sync(
    HttpMethod::type method, const std::string &uri,
    const std::string &request_body /* = {} */,
    const std::string &content_type /* = "application/json" */) {
  HttpRequest req{HttpRequest::sync_callback, nullptr};

  // TRACE forbids a request-body
  if (!request_body.empty()) {
    if (method == HttpMethod::Trace) {
      throw std::logic_error("TRACE can't have request-body");
    }
    req.get_output_headers().add("Content-Type", content_type.c_str());
    auto out_buf = req.get_output_buffer();
    out_buf.add(request_body.data(), request_body.size());
  }

  // ask the server to close the connection after this request
  req.get_output_headers().add("Connection", "close");
  req.get_output_headers().add("Host", hostname_.c_str());
  http_client_.make_request_sync(&req, method, uri);

  return req;
}
