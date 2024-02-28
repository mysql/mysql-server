/*
  Copyright (c) 2024, Oracle and/or its affiliates.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have include d with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTER_SRC_HTTP_INCLUDE_HTTP_CLIENT_REQUEST_H_
#define ROUTER_SRC_HTTP_INCLUDE_HTTP_CLIENT_REQUEST_H_

#include <memory>
#include <string>
#include <utility>

#include "http/base/request.h"

#include "mysqlrouter/http_client_export.h"

namespace http {
namespace client {

class HTTP_CLIENT_EXPORT Request : public http::base::Request {
 public:
  using IOBuffer = http::base::IOBuffer;
  using Headers = http::base::Headers;
  using Uri = http::base::Uri;
  using MethodType = http::base::method::key_type;
  using StatusType = http::base::status_code::key_type;
  using ConnectionInterface = http::base::ConnectionInterface;

 public:
  explicit Request(const Uri &uri, MethodType method = http::base::method::Get)
      : holder_{std::make_unique<Holder>(uri, method)} {}

  Request() : Request(Uri{"/"}, http::base::method::Get) {}
  Request(Request &&);
  ~Request() override;

  // Informations that we received from other side
  // In case of:
  // * server, they are Request data
  // * client, they are Response data
  const Headers &get_input_headers() const override;
  IOBuffer &get_input_buffer() const override;
  const std::string &get_input_body() const override;

  Headers &get_output_headers() override;
  IOBuffer &get_output_buffer() override;

  StatusType get_response_code() const override;
  std::string get_response_code_line() const;

  void set_method(MethodType) override;
  MethodType get_method() const override;

  const Uri &get_uri() const override;
  void set_uri(const Uri &uri) override;
  void set_uri(Uri &&uri) override;

  ConnectionInterface *get_connection() const override;

  operator bool() const {
    if (!holder_) return false;
    return (holder_->status >= 0);
  }

  std::string error_msg() const {
    if (*this) return {};

    return holder_->status_text;
  }

 private:
  friend class Client;

  class Holder {
   public:
    Holder(const Uri &u, MethodType m) : uri{u}, method{m} {}

    Headers headers_input;
    Headers headers_output;
    mutable IOBuffer buffer_input;
    IOBuffer buffer_output;
    Uri uri;
    MethodType method;
    ConnectionInterface *connection_interface{nullptr};
    // Following two variables, may hold either HTTP status
    // or error codes with text message.
    StatusType status{0};
    std::string status_text;
  };

  std::unique_ptr<Holder> holder_;
};

}  // namespace client
}  // namespace http

#endif  // ROUTER_SRC_HTTP_INCLUDE_HTTP_CLIENT_REQUEST_H_
