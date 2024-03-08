/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

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

#include "http/server/request.h"

#include <string_view>
#include <utility>

#include "http/base/http_time.h"
#include "http/base/status_code.h"
#include "http/base/uri.h"
#include "m_string.h"  // NOLINT(build/include_subdir)

std::string_view k_err_html_response_format{
    "<HTML><HEAD>\n"
    "<TITLE>%d %s</TITLE>\n"
    "</HEAD><BODY>\n"
    "<H1>%s</H1>\n"
    "</BODY></HTML>\n"};

namespace http {
namespace server {

using Headers = ServerRequest::Headers;
using IOBuffer = ServerRequest::IOBuffer;

ServerRequest::ServerRequest(ConnectionInterface *connection,
                             const uint32_t session_id,
                             const base::method::key_type method,
                             const std::string &path, Headers &&headers)
    : uri_{path}, connection_{connection}, holder_{session_id,
                                                   method,
                                                   std::move(headers),
                                                   {},
                                                   {},
                                                   {}} {}

Headers &ServerRequest::get_output_headers() { return holder_.output_headers_; }

const Headers &ServerRequest::get_input_headers() const {
  return holder_.input_headers_;
}

const std::string &ServerRequest::get_input_body() const {
  return holder_.input_body_.get();
}

IOBuffer &ServerRequest::get_input_buffer() const {
  return holder_.input_body_;
}

IOBuffer &ServerRequest::get_output_buffer() { return holder_.output_body_; }

void ServerRequest::send_error(StatusType status_code) {
  send_error(status_code, base::status_code::to_string(status_code));
}

void ServerRequest::send_error(StatusType status_code,
                               const std::string &status_text) {
  IOBuffer io_buffer;
  std::string &buffer = io_buffer.get();
  buffer.resize(k_err_html_response_format.size() + 2 * status_text.length() +
                40);

  const auto size =
      snprintf(&buffer[0], buffer.size(), k_err_html_response_format.data(),
               status_code, status_text.c_str(), status_text.c_str());

  assert(0 != size &&
         "This version of sprintf, may fail because of insufficient memory.");
  holder_.output_headers_.add("Content-Type", "text/html");

  buffer.resize(size);
  send_reply(status_code, status_text, io_buffer);
}

void ServerRequest::send_reply(StatusType status_code) {
  send_reply(status_code, base::status_code::to_string(status_code));
}

void ServerRequest::send_reply(StatusType status_code,
                               const std::string &status_text) {
  static const IOBuffer k_empty;
  send_reply(status_code, status_text, k_empty);
}

void ServerRequest::send_reply(StatusType status_code,
                               const std::string &status_text,
                               const IOBuffer &buffer) {
  using namespace std::literals;  // NOLINT(build/namespaces_literals)

  static std::string k_path;

  auto value = holder_.input_headers_.find("Connection");
  if (value) {
    if (http::base::compare_case_insensitive(*value, "Keep-Alive"sv)) {
      holder_.output_headers_.add("Connection", "Keep-Alive");
    } else if (http::base::compare_case_insensitive(*value, "close"sv)) {
      holder_.output_headers_.add("Connection", "close");
    }
  }

  holder_.output_headers_.add("Content-Length",
                              std::to_string(buffer.length()));
  connection_->send(&holder_.stream_id_, status_code, status_text, k_path,
                    holder_.output_headers_, buffer);
}

base::method::key_type ServerRequest::get_method() const {
  return holder_.method_;
}

const base::Uri &ServerRequest::get_uri() const { return uri_; }

bool ServerRequest::is_modified_since(time_t last_modified) {
  auto *value = holder_.input_headers_.find("If-Modified-Since");

  if (value) {
    try {
      time_t if_mod_since_ts =
          http::base::time_from_rfc5322_fixdate(value->c_str());

      if (!(last_modified > if_mod_since_ts)) {
        return false;
      }
    } catch (const std::exception &) {
      return false;
    }
  }
  return true;
}

bool ServerRequest::add_last_modified(time_t last_modified) {
  char date_buf[50];

  auto filled = http::base::time_to_rfc5322_fixdate(last_modified, date_buf,
                                                    sizeof(date_buf));

  if (filled > 0 && sizeof(date_buf) > static_cast<size_t>(filled)) {
    holder_.output_headers_.add("Last-Modified", date_buf);

    return true;
  } else {
    return false;
  }
}

}  // namespace server
}  // namespace http
