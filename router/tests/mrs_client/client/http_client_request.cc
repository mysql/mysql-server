/*
  Copyright (c) 2023, 2026, Oracle and/or its affiliates.

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

#include "client/http_client_request.h"

#include <iostream>
#include <utility>

#include "mysql/harness/string_utils.h"

namespace mrs_client {

HttpClientRequest::HttpClientRequest(net::io_context *context,
                                     HttpClientSession *session,
                                     const http::base::Uri &uri,
                                     const bool use_http2)
    : uri_{uri}, context_{context}, session_{session} {
  TlsClientContext context_tls{TlsVerify::NONE};
  key_dump_ = std::make_unique<tls::TlsKeylogDumper>(context_tls.get());
  client_ = std::make_unique<http::client::Client>(
      *context_, std::move(context_tls), use_http2);
}

void HttpClientRequest::add_header(const char *name, const char *value) {
  one_shot_headers_.emplace_back(name, value);
}

static Headers get_headers(const ::http::base::Request *request) {
  Headers result;
  auto &headers = request->get_input_headers();
  for (const auto &h : headers) {
    result.push_back(h);
  }

  return result;
}

const Headers &HttpClientRequest::get_input_headers() const {
  return input_headers_;
}

Result HttpClientRequest::do_request(http::base::method::key_type method,
                                     const std::string &path,
                                     const std::string &body,
                                     bool set_new_cookies) {
  http::base::Uri u_path{path};

  // Move just the parsed path part.
  uri_.set_path(u_path.get_path());
  uri_.set_query(u_path.get_query());
  uri_.set_fragment(u_path.get_fragment());

  http::client::Request request{uri_, method};

  auto &oheaders = request.get_output_headers();
  auto &obuff = request.get_output_buffer();

  for (const auto &kv : one_shot_headers_) {
    oheaders.add(kv.first.c_str(), kv.second.c_str());
  }
  if (session_) session_->fill_request_headers(&oheaders);

  one_shot_headers_.clear();

  obuff.add(body.c_str(), body.length());

  context_->restart();
  client_->send_request(&request);

  if (client_->error_code()) {
    throw std::runtime_error(client_->error_message());
  }

  if (session_ && set_new_cookies)
    session_->analyze_response_headers(&request.get_input_headers());

  auto code =
      static_cast<HttpStatusCode::key_type>(request.get_response_code());
  auto &ib = request.get_input_buffer();
  auto array = ib.pop_front(ib.length());

  input_headers_ = get_headers(&request);
  return Result{code, input_headers_, std::string(array.begin(), array.end())};
}

}  // namespace mrs_client
