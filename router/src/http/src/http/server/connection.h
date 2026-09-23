/*
  Copyright (c) 2024, 2026, Oracle and/or its affiliates.

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

#include <charconv>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "http/base/connection.h"
#include "http/base/headers.h"
#include "http/server/http_counters.h"
#include "http/server/request.h"
#include "http/server/request_handler_interface.h"
#include "mysqlrouter/http_server_lib_export.h"
#include "mysqlrouter/uri.h"

namespace http {
namespace server {

template <typename Socket>
class ServerConnection : public http::base::Connection<Socket> {
 public:
  using Parent = http::base::Connection<Socket>;
  using SessionId = uint32_t;

  class HTTP_SERVER_LIB_EXPORT ConnectionStatusCallbacks
      : public Parent::ConnectionStatusCallbacks {
   public:
    virtual uint64_t max_request_body_size() const = 0;
    virtual uint64_t max_response_body_size() const = 0;
    virtual void log_max_request_body_size_rejection(
        uint64_t max_request_body_size,
        std::optional<uint64_t> content_length) = 0;
    virtual void log_max_response_body_size_rejection(
        uint64_t max_response_body_size, uint64_t response_body_size) = 0;
  };

 public:
  struct BodyHeadersValidationResult {
    bool has_transfer_encoding{false};
    bool has_content_length{false};
    bool invalid_content_length{false};
    std::optional<uint64_t> content_length;
  };

  ServerConnection(Socket s, base::method::Bitset *allowed_method,
                   RequestHandlerInterface *rhi,
                   ConnectionStatusCallbacks *connection_handler)
      : Parent(std::move(s), allowed_method, connection_handler,
               CNO_CONNECTION_KIND::CNO_SERVER, CNO_HTTP_VERSION::CNO_HTTP1),
        request_handler_{rhi},
        connection_handler_{connection_handler} {}

  static std::optional<uint64_t> parse_content_length(
      std::string_view content_length_value) {
    uint64_t content_length{};
    const auto *begin = content_length_value.data();
    const auto *end = begin + content_length_value.size();
    const auto [ptr, ec] = std::from_chars(begin, end, content_length);
    if (ec != std::errc{} || ptr != end) return std::nullopt;
    return content_length;
  }

  static bool body_size_exceeds_limit(uint64_t max_size, uint64_t current_size,
                                      size_t next_chunk_size) {
    if (current_size > max_size) return true;
    return next_chunk_size > (max_size - current_size);
  }

  static bool body_size_exceeds_limit(uint64_t limit, size_t body_size) {
    return body_size_exceeds_limit(limit, 0, body_size);
  }

  static void update_body_related_header(std::string_view name,
                                         std::string_view value,
                                         BodyHeadersValidationResult &result) {
    if (http::base::compare_case_insensitive(std::string{name},
                                             "Transfer-Encoding")) {
      result.has_transfer_encoding = true;
      return;
    }

    if (!http::base::compare_case_insensitive(std::string{name},
                                              "Content-Length")) {
      return;
    }

    result.has_content_length = true;

    const auto parsed = parse_content_length(value);
    if (!parsed.has_value()) {
      result.invalid_content_length = true;
      return;
    }

    if (!result.content_length.has_value()) {
      result.content_length = *parsed;
    } else if (*result.content_length != *parsed) {
      result.invalid_content_length = true;
    }
  }

  bool send(const uint32_t *stream_id_ptr, const int status_code,
            const std::string &method, const std::string &path,
            const typename Parent::Headers &headers,
            const http::base::IOBuffer &data) override {
    const auto max_response_body_size =
        connection_handler_->max_response_body_size();
    if (body_size_exceeds_limit(max_response_body_size, data.length())) {
      connection_handler_->log_max_response_body_size_rejection(
          max_response_body_size, data.length());

      typename Parent::Headers error_headers;
      static const http::base::IOBuffer k_empty;
      error_headers.add("Connection", "close");
      error_headers.add("Content-Length", "0");
      Parent::keep_alive_ = false;
      return Parent::send(
          stream_id_ptr, base::status_code::InternalError,
          base::status_code::to_string(base::status_code::InternalError), path,
          error_headers, k_empty);
    }

    return Parent::send(stream_id_ptr, status_code, method, path, headers,
                        data);
  }

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
    auto it = sessions_.find(session_id);
    if (it == sessions_.end()) {
      return 1;
    }
    auto &session = it->second;
    if (session.payload_limit_exceeded) {
      return 1;
    }

    if (body_size_exceeds_limit(session.max_request_body_size,
                                session.input_body_size, size)) {
      connection_handler_->log_max_request_body_size_rejection(
          session.max_request_body_size, std::nullopt);
      session.request.send_reply(base::status_code::PayloadTooLarge);
      session.payload_limit_exceeded = true;
      Parent::keep_alive_ = false;
      return 1;
    }

    session.input_body_size += size;
    session.request.get_data().input_body_.get().append(data, size);
    return 0;
  }

  int on_cno_message_tail(const uint32_t session_id,
                          [[maybe_unused]] const cno_tail_t *tail) override {
    auto it = sessions_.find(session_id);
    if (it == sessions_.end() || it->second.payload_limit_exceeded) {
      return 1;
    }

    if (request_handler_) {
      request_handler_->route(it->second.request);
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

    BodyHeadersValidationResult body_headers{};
    // libcno may normalize HTTP/1 request framing headers before exposing the
    // header list, so use parser-provided presence flags for conflict checks.
    body_headers.has_transfer_encoding = msg->has_transfer_encoding != 0;
    body_headers.has_content_length = msg->has_content_length != 0;
    for (const auto &header : sequence) {
      auto header_name = cno::to_string(header.name);
      auto header_value = cno::to_string(header.value);
      update_body_related_header(header_name, header_value, body_headers);
      input_headers.add(std::move(header_name), std::move(header_value));
    }
    if (body_headers.has_transfer_encoding && body_headers.has_content_length) {
      connection_handler_->log_invalid_request_body_headers_rejection(
          "both Content-Length and Transfer-Encoding are present");
      ServerRequest(this, session_id, (base::method::key_type)(1 << method_pos),
                    path, std::move(input_headers))
          .send_reply(base::status_code::BadRequest);
      Parent::keep_alive_ = false;
      return 1;
    }

    if (body_headers.invalid_content_length) {
      connection_handler_->log_invalid_request_body_headers_rejection(
          "invalid Content-Length");
      ServerRequest(this, session_id, (base::method::key_type)(1 << method_pos),
                    path, std::move(input_headers))
          .send_reply(base::status_code::BadRequest);
      Parent::keep_alive_ = false;
      return 1;
    }

    const auto max_request_body_size =
        connection_handler_->max_request_body_size();
    if (body_headers.content_length.has_value() &&
        *body_headers.content_length > max_request_body_size) {
      connection_handler_->log_max_request_body_size_rejection(
          max_request_body_size, body_headers.content_length);
      ServerRequest(this, session_id, (base::method::key_type)(1 << method_pos),
                    path, std::move(input_headers))
          .send_reply(base::status_code::PayloadTooLarge);
      Parent::keep_alive_ = false;
      return 1;
    }

    if (!(*Parent::allowed_method_)[method_pos]) {
      ServerRequest(this, session_id, (base::method::key_type)(1 << method_pos),
                    "", std::move(input_headers))
          .send_error(base::status_code::NotImplemented);
      return 1;
    }

    sessions_.erase(session_id);
    try {
      auto pair = sessions_.try_emplace(
          session_id, this, session_id,
          (base::method::key_type)(1 << method_pos), path,
          std::move(input_headers), max_request_body_size);

      char buffer[90];
      http::base::time_to_rfc5322_fixdate(time(nullptr), buffer,
                                          sizeof(buffer));
      pair.first->second.request.get_output_headers().add("Date", buffer);
      pair.first->second.request.get_output_headers().add(
          "Content-Type", "text/html; charset=ISO-8859-1");

    } catch (...) {
      ServerRequest(this, session_id, (base::method::key_type)(1 << method_pos),
                    "", std::move(input_headers))
          .send_error(base::status_code::BadRequest);
      return 1;
    }

    return 0;
  }

  bool first_request_{true};
  struct SessionData {
    SessionData(http::base::ConnectionInterface *connection,
                const uint32_t session_id, const base::method::key_type method,
                const std::string &path, http::base::Headers &&headers,
                uint64_t max_body_size)
        : request(connection, session_id, method, path, std::move(headers)),
          max_request_body_size(max_body_size) {}

    ServerRequest request;
    uint64_t max_request_body_size;
    uint64_t input_body_size{0};
    bool payload_limit_exceeded{false};
  };
  std::map<SessionId, SessionData> sessions_;
  RequestHandlerInterface *request_handler_;
  ConnectionStatusCallbacks *connection_handler_;
};

}  // namespace server
}  // namespace http

#endif  // ROUTER_SRC_HTTP_SRC_HTTP_SERVER_CONNECTION_H_
