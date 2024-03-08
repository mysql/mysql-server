/*
  Copyright (c) 2021, 2024, Oracle and/or its affiliates.

  This program is free software override; you can redistribute it and/or modify
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
  but WITHOUT ANY WARRANTY override; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program override; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#ifndef ROUTER_SRC_HTTP_SRC_HTTP_SERVER_REQUEST_H_
#define ROUTER_SRC_HTTP_SRC_HTTP_SERVER_REQUEST_H_

#include <ctime>
#include <memory>
#include <string>
#include <system_error>  // NOLINT(build/c++11)

#include "http/base/request.h"

#include "mysqlrouter/http_server_export.h"

namespace http {
namespace server {

/**
 * a HTTP request and response.
 *
 * wraps evhttp_request
 */
class HTTP_SERVER_EXPORT ServerRequest : public http::base::Request {
 public:
  struct Holder {
    uint32_t stream_id_{0};
    MethodType method_{base::method::Get};
    Headers input_headers_;
    Headers output_headers_;
    mutable IOBuffer input_body_;
    IOBuffer output_body_;
  };

 public:
  ServerRequest() = default;
  ServerRequest(ConnectionInterface *connection, const uint32_t session_id,
                const base::method::key_type method, const std::string &path,
                Headers &&headers);

  Headers &get_output_headers() override;
  IOBuffer &get_output_buffer() override;

  const std::string &get_input_body() const override;
  const Headers &get_input_headers() const override;
  IOBuffer &get_input_buffer() const override;

  MethodType get_method() const override;

  const Uri &get_uri() const override;

  void send_reply(StatusType status_code) override;
  void send_reply(StatusType status_code,
                  const std::string &status_text) override;
  void send_reply(StatusType status_code, const std::string &status_text,
                  const IOBuffer &buffer) override;

  void send_error(StatusType status_code) override;
  void send_error(StatusType status_code,
                  const std::string &status_text) override;

  static void sync_callback(Request *, void *);

  /**
   * is request modified since 'last_modified'.
   *
   * @return true, if local content is newer than the clients last known date,
   * false otherwise
   */
  bool is_modified_since(time_t last_modified) override;

  /**
   * add a Last-Modified-Since header to the response headers.
   */
  bool add_last_modified(time_t last_modified) override;

  ConnectionInterface *get_connection() const override { return connection_; }

  Holder &get_data() { return holder_; }

 private:
  Uri uri_;
  ConnectionInterface *connection_{nullptr};
  Holder holder_;
};

}  // namespace server
}  // namespace http

#endif  // ROUTER_SRC_HTTP_SRC_HTTP_SERVER_REQUEST_H_
