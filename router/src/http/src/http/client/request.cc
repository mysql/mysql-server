/*
  Copyright (c) 2024, Oracle and/or its affiliates.

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

#include "http/client/request.h"

#include <utility>

namespace http {
namespace client {

using IOBuffer = Request::IOBuffer;
using MethodType = Request::MethodType;
using Headers = Request::Headers;
using StatusType = Request::StatusType;
using Uri = Request::Uri;
using ConnectionInterface = Request::ConnectionInterface;

Request::Request(Request &&other) : holder_{std::move(other.holder_)} {}

Request::~Request() = default;

Headers &Request::get_output_headers() { return holder_->headers_output; }

const Headers &Request::get_input_headers() const {
  return holder_->headers_input;
}

const std::string &Request::get_input_body() const {
  return holder_->buffer_input.get();
}

IOBuffer &Request::get_input_buffer() const { return holder_->buffer_input; }

IOBuffer &Request::get_output_buffer() { return holder_->buffer_output; }

ConnectionInterface *Request::get_connection() const {
  return holder_->connection_interface;
}

StatusType Request::get_response_code() const { return holder_->status; }

std::string Request::get_response_code_line() const {
  return holder_->status_text;
}

MethodType Request::get_method() const { return holder_->method; }

void Request::set_method(MethodType method) { holder_->method = method; }

const Uri &Request::get_uri() const { return holder_->uri; }

void Request::set_uri(Uri &&uri) { holder_->uri = std::move(uri); }

void Request::set_uri(const Uri &uri) { holder_->uri = uri; }

}  // namespace client
}  // namespace http
