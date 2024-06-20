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

#ifndef ROUTER_SRC_HTTP_SRC_HTTP_SERVER_CONNECTION_H_
#define ROUTER_SRC_HTTP_SRC_HTTP_SERVER_CONNECTION_H_

#include <map>
#include <utility>

#include "http/base/connection.h"
#include "http/server/http_counters.h"
#include "http/server/request.h"
#include "http/server/request_handler_interface.h"

namespace http {
namespace server {

template <typename Socket>
class ServerConnection : public http::base::Connection<Socket> {
 public:
  using Parent = http::base::Connection<Socket>;
  using SessionId = uint32_t;
  using ConnectionStatusCallbacks = typename Parent::ConnectionStatusCallbacks;

 public:
  ServerConnection(Socket s, base::method::Bitset *allowed_method,
                   RequestHandlerInterface *rhi,
                   ConnectionStatusCallbacks *connection_handler)
      : Parent(std::move(s), allowed_method, connection_handler,
               CNO_CONNECTION_KIND::CNO_SERVER, CNO_HTTP_VERSION::CNO_HTTP1),
        request_handler_{rhi} {}

 private:
  int on_settings() override {
    // Server doesn't need to synchronize to settings, it receives settings as
    // part of the request.
    return 0;
  }

  int on_cno_message_body(const uint32_t session_id, const char *data,
                          const size_t size) override {
    // We can blindly use session_id with the map because
    // the map was already initialized in `on_cno_message_head` call.
    // The 'cno' executes callbacks in following order:
    //
    // * on_cno_message_head
    // * on_cno_message_body
    // * on_cno_message_tail
    // * on_cno_stream_end
    sessions_[session_id].get_data().input_body_.get().append(data, size);
    return 0;
  }

  int on_cno_message_tail(const uint32_t session_id,
                          [[maybe_unused]] const cno_tail_t *tail) override {
    if (request_handler_) {
      request_handler_->route(sessions_[session_id]);
    }

    return 0;
  }

  int on_cno_stream_end(const uint32_t id) override {
    sessions_.erase(id);
    return 0;
  }

  int on_cno_message_head(const uint32_t session_id,
                          const cno_message_t *msg) override {
    if (!first_request_) http_connections_reused++;
    first_request_ = false;
    const auto method_pos =
        base::method::from_string_to_post(cno::to_string(msg->method));

    http::base::Headers input_headers;
    const auto path = cno::to_string(msg->path);
    cno::Sequence<const cno_header_t> sequence{msg->headers, msg->headers_len};

    for (const auto &header : sequence) {
      input_headers.add(cno::to_string(header.name),
                        cno::to_string(header.value));
    }

    if (!(*Parent::allowed_method_)[method_pos]) {
      ServerRequest(this, session_id, (base::method::key_type)(1 << method_pos),
                    path, std::move(input_headers))
          .send_error(base::status_code::NotImplemented);
      return 1;
    }

    sessions_.erase(session_id);
    auto pair = sessions_.try_emplace(session_id, this, session_id,
                                      (base::method::key_type)(1 << method_pos),
                                      path, std::move(input_headers));

    char buffer[90];
    http::base::time_to_rfc5322_fixdate(time(nullptr), buffer, sizeof(buffer));
    pair.first->second.get_output_headers().add("Date", buffer);
    pair.first->second.get_output_headers().add(
        "Content-Type", "text/html; charset=ISO-8859-1");

    return 0;
  }

  bool first_request_{true};
  std::map<SessionId, ServerRequest> sessions_;
  RequestHandlerInterface *request_handler_;
};

}  // namespace server
}  // namespace http

#endif  // ROUTER_SRC_HTTP_SRC_HTTP_SERVER_CONNECTION_H_
