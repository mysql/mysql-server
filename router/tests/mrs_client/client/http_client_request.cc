/*
  Copyright (c) 2023, Oracle and/or its affiliates.

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

#include "client/http_client_request.h"

#include <iostream>

#include "configuration/url.h"
#include "mysql/harness/string_utils.h"
#include "mysqlrouter/http_request.h"

namespace mrs_client {

static Headers get_headers(HttpRequest &request) {
  Headers result;
  auto &headers = request.get_input_headers();
  for (const auto &h : headers) {
    result.push_back(h);
  }

  return result;
}

HttpClientRequest::HttpClientRequest(IOContext *context,
                                     HttpClientSession *session, Url *url)
    : url_{url}, context_{context}, session_{session} {
  create_request();
}

void HttpClientRequest::create_request() {
  if (!url_->needs_tls()) {
    connection_.reset(new HttpClientConnection(*context_, url_->get_host(),
                                               url_->get_port()));
  } else {
    connection_.reset(new HttpsClientConnection(
        *context_, ctx_tls_, url_->get_host(), url_->get_port()));
  }
}

void HttpClientRequest::add_header(const char *name, const char *value) {
  one_shot_headers_.emplace_back(name, value);
}

Result HttpClientRequest::do_request(HttpMethod::type type,
                                     const std::string &path,
                                     const std::string &body [[maybe_unused]]) {
  HttpRequestImpl http_request{
      [](HttpRequestImpl *impl, void *ctxt) {
        auto This = reinterpret_cast<HttpClientRequest *>(ctxt);
        HttpRequestImpl::sync_callback(impl, ctxt);
        This->context_->do_break();
      },
      this};

  auto &oheaders = http_request.get_output_headers();
  auto &obuff = http_request.get_output_buffer();

  for (const auto &kv : one_shot_headers_) {
    oheaders.add(kv.first.c_str(), kv.second.c_str());
  }
  if (session_) session_->fill_request_headers(&oheaders);

  one_shot_headers_.clear();

  obuff.add(body.c_str(), body.length());
  connection_->make_request_sync(&http_request, type, path);

  if (http_request.error_code()) {
    throw std::runtime_error("Can't connect to remote endpoint");
  }

  if (session_)
    session_->analyze_response_headers(&http_request.get_input_headers());

  auto code =
      static_cast<HttpStatusCode::key_type>(http_request.get_response_code());
  auto &ib = http_request.get_input_buffer();
  auto array = ib.pop_front(ib.length());

  return Result{code, get_headers(http_request),
                std::string(array.begin(), array.end())};
}

}  // namespace mrs_client
