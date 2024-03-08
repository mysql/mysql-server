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

#ifndef ROUTER_SRC_HTTP_INCLUDE_HTTP_CLIENT_CONNECTION_H_
#define ROUTER_SRC_HTTP_INCLUDE_HTTP_CLIENT_CONNECTION_H_

#include <string>
#include <utility>

#include "http/base/connection.h"
#include "http/client/payload_callback.h"
#include "http/client/request.h"

#include "mysqlrouter/http_client_export.h"

namespace http {
namespace client {

template <typename IOLayer>
class HTTP_CLIENT_EXPORT Connection : public http::base::Connection<IOLayer> {
 public:
  using Parent = http::base::Connection<IOLayer>;
  using IOBuffer = typename Parent::IOBuffer;
  using Headers = typename Parent::Headers;
  using ConnectionStatusCallbacks = typename Parent::ConnectionStatusCallbacks;

 public:
  Connection(IOLayer s, base::method::Bitset *allowed_method,
             ConnectionStatusCallbacks *connection_handler,
             PayloadCallback *payload_callback)
      : Parent::Connection(std::move(s), allowed_method, connection_handler,
                           CNO_CONNECTION_KIND::CNO_CLIENT),
        payload_{payload_callback} {}

 public:  // override CnoInterface
  int on_cno_message_body([[maybe_unused]] const uint32_t session_id,
                          const char *data, const size_t size) override {
    payload_->on_input_payload(data, size);
    return 0;
  }

  int on_cno_message_tail([[maybe_unused]] const uint32_t session_id,
                          [[maybe_unused]] const cno_tail_t *tail) override {
    Parent::suspend();
    payload_->on_input_end();

    return 0;
  }

  bool send(const uint32_t *stream_id_ptr, const int status_code,
            const std::string &method, const std::string &path,
            const Headers &headers, const IOBuffer &data) override {
    Parent::resume();
    return Parent::send(stream_id_ptr, status_code, method, path, headers,
                        data);
  }

  int on_cno_stream_end([[maybe_unused]] const uint32_t id) override {
    return 0;
  }

  int on_cno_message_head([[maybe_unused]] const uint32_t session_id,
                          const cno_message_t *msg) override {
    payload_->on_input_begin(msg->code,
                             std::string{msg->method.data, msg->method.size});

    http::base::Headers input_headers;
    const auto path = cno::to_string(msg->path);
    cno::Sequence<const cno_header_t> sequence{msg->headers, msg->headers_len};

    for (const auto &header : sequence) {
      payload_->on_input_header(cno::to_string(header.name).c_str(),
                                cno::to_string(header.value).c_str());
    }

    return 0;
  }

  void on_output_buffer_empty() override { payload_->on_output_end_payload(); }

 public:
  PayloadCallback *payload_;
};

}  // namespace client
}  // namespace http

#endif  // ROUTER_SRC_HTTP_INCLUDE_HTTP_CLIENT_CONNECTION_H_
