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

#include <functional>
#include <iostream>      // cout
#include <system_error>  // error_code
#include <utility>

#include "http/base/uri.h"
#include "http/client/client.h"
#include "http/client/connection.h"
#include "http/client/error_code.h"
#include "http/client/payload_callback.h"

#include "router_config.h"  // NOLINT(build/include_subdir)
#include "tls/tls_stream.h"
#include "tls/trace_stream.h"

namespace http {
namespace client {

const std::string k_http = "http";
const std::string k_https = "https";

class ConsoleRawOut {
 public:
  static std::ostream *get_out() { return &std::cout; }
  static const char *get_name() { return "RAW"; }
};

class ConsoleSocketOut {
 public:
  static std::ostream *get_out() { return &std::cout; }
  static const char *get_name() { return "SOCK"; }
};

using Socket =  // Choose one of following.
    net::ip::tcp::socket;
//    TraceStream<ConsoleRawOut, net::ip::tcp::socket>;
using TlsSocket =  // Choose one of following.
    net::tls::TlsStream<net::ip::tcp::socket>;
// TraceStream<ConsoleRawOut, net::tls::TlsStream<TraceStream<
//                                ConsoleSocketOut, net::ip::tcp::socket>>>;
using ConnectionTls = http::client::Connection<TlsSocket>;
using ConnectionRaw = http::client::Connection<Socket>;

namespace impl {

struct ConfigSchema {
  bool is_tls;
  uint16_t port;
};

using TlsHandshakeCallback =
    std::function<void(const std::error_code &ec, const size_t no_of_bytes)>;
using TlsHandshakeExecute = std::function<void(TlsHandshakeCallback)>;

struct Connection {
  net::ip::tcp::socket *socket;
  std::unique_ptr<http::base::ConnectionInterface> connection;
  TlsHandshakeExecute tls_handshake_execute;
};

static void headers_add_if_not_present(http::base::Headers *h, const char *key,
                                       const char *value) {
  auto result = h->find(key);
  if (result) return;

  h->add(key, value);
}

template <typename V>
V value_or(V value_users, V value_default) {
  if (value_users.empty()) return value_default;
  return value_users;
}

static Client::Endpoint get_endpoint_from(const http::base::Uri &url) {
  static const std::map<std::string, impl::ConfigSchema> k_protocol_ports{
      {k_http, {false, 80}}, {k_https, {true, 8080}}};
  Client::Endpoint result;

  auto scheme = value_or(url.get_scheme(), k_http);

  auto port_it = k_protocol_ports.find(scheme);
  if (port_it == k_protocol_ports.end()) {
    throw make_error_code(FailureCode::kInvalidScheme);
  }

  result.port = url.get_port();
  result.port = result.port > 0 ? result.port : port_it->second.port;
  result.is_tls = port_it->second.is_tls;
  result.host = url.get_host();

  // Remove the URL notation for ipv6 addresses.
  if (!result.host.empty()) {
    if (*result.host.begin() == '[' && *result.host.rbegin() == ']') {
      result.host = result.host.substr(1, result.host.length() - 2);
    }
  }

  return result;
}

static const std::string &get_method_as_string(
    http::base::method::key_type method) {
  // Use the 'namespace' for readability of enumeration of
  // method types in std::map.
  using namespace http::base;  // NOLINT(build/namespaces)

  // The performance at this point is not important.
  static const std::map<method::key_type, std::string> method_map{
      {method::Get, "GET"},       {method::Post, "POST"},
      {method::Head, "HEAD"},     {method::Put, "PUT"},
      {method::Delete, "DELETE"}, {method::Options, "OPTIONS"},
      {method::Trace, "TRACE"},   {method::Connect, "CONNECT"},
      {method::Patch, "PATCH"},
  };

  auto it = method_map.find(method);

  if (it == method_map.end()) {
    throw make_error_code(FailureCode::kUnknowHttpMethod);
  }

  return it->second;
}

// `CallbakcsPrivateImpl` class is declared as private and
// its implemented later one.
// To workaround the mentioned limitations, here we use it
// as templated type.
template <typename ConnectionStatusCallback>
impl::Connection create_connection_object(
    net::io_context &io_context, bool is_tls, TlsClientContext *tls_context,
    ConnectionStatusCallback *ccs, PayloadCallback *obj, bool use_http2) {
  impl::Connection result;

  if (is_tls) {
    auto conn = std::make_unique<ConnectionTls>(
        TlsSocket{tls_context, net::ip::tcp::socket{io_context}}, nullptr, ccs,
        obj, use_http2);

    result.tls_handshake_execute = [con = conn.get()](
                                       TlsHandshakeCallback callback) {
      con->get_socket().async_handshake(net::tls::kClient, std::move(callback));
    };
    result.socket = http::base::impl::get_socket(&conn->get_socket());
    result.connection = std::move(conn);

    return result;
  }

  auto conn = std::make_unique<ConnectionRaw>(Socket{io_context}, nullptr, ccs,
                                              obj, use_http2);
  result.socket = http::base::impl::get_socket(&conn->get_socket());
  result.connection = std::move(conn);

  return result;
}

}  // namespace impl

PayloadCallback::~PayloadCallback() = default;

class Client::CallbacksPrivateImpl
    : public PayloadCallback,
      public ConnectionTls::ConnectionStatusCallbacks,
      public ConnectionRaw::ConnectionStatusCallbacks {
 public:
  explicit CallbacksPrivateImpl(Client *client) : parent_{client} {}

 public:  // PayloadCallback
  void on_connection_ready() override;
  void on_input_payload(const char *data, size_t size) override;
  void on_input_begin(int status_code, const std::string &status_text) override;
  void on_input_end() override;
  void on_input_header(std::string &&key, std::string &&value) override;
  void on_output_end_payload() override;

 public:  // ConnectionTls::ConnectionStatusCallbacks
  void on_connection_close(ConnectionTls::Parent *connection) override;
  void on_connection_io_error(ConnectionTls::Parent *connection,
                              const std::error_code &ec) override;

 public:  // ConnectionRaw::ConnectionStatusCallbacks
  void on_connection_close(ConnectionRaw::Parent *connection) override;
  void on_connection_io_error(ConnectionRaw::Parent *connection,
                              const std::error_code &ec) override;

 private:
  Client *parent_;
};

Client::Client(io_context &io_context, TlsClientContext &&tls_context,
               bool use_http2)
    : io_context_{io_context},
      tls_context_{std::move(tls_context)},
      callbacks_{std::make_unique<CallbacksPrivateImpl>(this)},
      use_http2_{use_http2} {}

Client::Client(io_context &io_context, const bool use_http2)
    : io_context_{io_context},
      callbacks_{std::make_unique<CallbacksPrivateImpl>(this)},
      use_http2_{use_http2} {}

Client::~Client() = default;

void Client::async_send_request(http::client::Request *request) {
  using namespace std::string_literals;

  try {
    const auto &url = request->get_uri();
    error_code_ = std::error_code();
    request->holder_->status = 0;
    request->holder_->status_text.clear();

    if (!url) throw make_error_code(FailureCode::kInvalidUrl);

    auto endpoint = impl::get_endpoint_from(url);

    if (endpoint.host.empty())
      throw make_error_code(FailureCode::kInvalidHostname);

    auto &headers = request->get_output_headers();
    headers.add("Host", std::string(endpoint.host));
    impl::headers_add_if_not_present(
        &headers, "User-Agent", "router-http-client/" MYSQL_ROUTER_VERSION);
    impl::headers_add_if_not_present(&headers, "Accept", "*/*");

    if (use_http2_) {
      // Pseudo headers must be at start of Header block.
      const std::string_view k_scheme_key_name{":scheme"};
      auto scheme_value = impl::value_or(url.get_scheme(), k_http);
      if (!headers.find(k_scheme_key_name)) {
        headers.insert(headers.begin(), k_scheme_key_name,
                       std::move(scheme_value));
      }
    }

    fill_request_by_callback_ = request;

    if (!is_connected_ || endpoint != connected_endpoint_) {
      net::ip::tcp::resolver resolv{io_context_};
      auto resolve_result =
          resolv.resolve(endpoint.host, std::to_string(endpoint.port));
      if (!resolve_result) throw make_error_code(FailureCode::kResolveFailure);

      if (resolve_result.value().empty())
        throw make_error_code(FailureCode::kResolveHostNotFound);

      auto connection_objects = impl::create_connection_object(
          io_context_, endpoint.is_tls, &tls_context_, callbacks_.get(),
          callbacks_.get(), use_http2_);

      auto connect = [&resolve_result,
                      &socket = connection_objects.socket]() -> bool {
        for (const auto &ainfo : resolve_result.value()) {
          auto ep = ainfo.endpoint();
          if (socket->connect(ep)) {
            return true;
          }
        }
        return false;
      };

      if (!connect()) throw make_error_code(FailureCode::kConnectionFailure);

      statistics_.connected++;
      if (endpoint.is_tls) statistics_.connected_tls++;
      connected_endpoint_ = endpoint;
      connection_ = std::move(connection_objects.connection);
      is_connected_ = true;

      if (endpoint.is_tls) {
        connection_objects.tls_handshake_execute(
            [this](const std::error_code &ec,
                   [[maybe_unused]] const size_t no_of_bytes) {
              if (!ec) {
                start_http_flow();
              } else {
                is_connected_ = false;
                error_code_ = ec;
                fill_request_by_callback_->holder_->status_text =
                    error_code_.message();
                fill_request_by_callback_->holder_->status = -1;
              }
            });
        return;
      }
    } else {
      statistics_.reused++;
    }

    start_http_flow();
  } catch (const std::error_code &e) {
    is_connected_ = false;
    error_code_ = e;
    request->holder_->status_text = error_code_.message();
    request->holder_->status = -1;
  }
}

void Client::start_http_flow() {
  if (!use_http2_) {
    auto request = fill_request_by_callback_;
    const auto &url = request->get_uri();
    const auto &method = impl::get_method_as_string(request->get_method());
    connection_->send(nullptr, 0, method, url.join_path(),
                      request->get_output_headers(),
                      request->get_output_buffer());
  } else {
    // Wait for "HTTP2 setting" exchange before sending the http request.
    connection_->start();
  }
}

void Client::send_request(http::client::Request *request) {
  async_send_request(request);
  io_context_.run();
}

Client::operator bool() const { return !error_code_; }

int Client::error_code() const { return error_code_.value(); }

std::string Client::error_message() const { return error_code_.message(); }

const Client::Statistics &Client::statistics() const { return statistics_; }

void Client::CallbacksPrivateImpl::on_input_begin(
    int status_code, [[maybe_unused]] const std::string &status_text) {
  auto *holder = parent_->fill_request_by_callback_->holder_.get();
  holder->status = status_code;
  holder->status_text = status_text;
  holder->headers_input.clear();
  holder->buffer_input.clear();
}

void Client::CallbacksPrivateImpl::on_input_end() {
  bool close_connection = false;
  auto &oh = parent_->fill_request_by_callback_->get_output_headers();
  auto &ih = parent_->fill_request_by_callback_->get_input_headers();
  auto oconn = oh.find("Connection");

  if (oconn && (*oconn == "close")) {
    close_connection = true;
  } else {
    auto iconn = ih.find("Connection");
    if (iconn && (*iconn == "close")) close_connection = true;
  }

  if (close_connection) {
    parent_->is_connected_ = false;
  }
}

void Client::CallbacksPrivateImpl::on_output_end_payload() {
  if (!parent_->use_http2_) parent_->connection_->start();
}

void Client::CallbacksPrivateImpl::on_input_header(std::string &&key,
                                                   std::string &&value) {
  parent_->fill_request_by_callback_->holder_->headers_input.add(
      std::move(key), std::move(value));
}

void Client::CallbacksPrivateImpl::on_connection_ready() {
  auto request = parent_->fill_request_by_callback_;
  const auto &url = request->get_uri();
  const auto &method = impl::get_method_as_string(request->get_method());
  parent_->connection_->send(nullptr, 0, method, url.join_path(),
                             request->get_output_headers(),
                             request->get_output_buffer());
}

void Client::CallbacksPrivateImpl::on_input_payload(const char *data,
                                                    size_t size) {
  parent_->fill_request_by_callback_->holder_->buffer_input.get().append(data,
                                                                         size);
}

void Client::CallbacksPrivateImpl::on_connection_close(
    ConnectionTls::Parent *connection) {
  connection->get_socket().close();
  parent_->is_connected_ = false;
}

void Client::CallbacksPrivateImpl::on_connection_io_error(
    [[maybe_unused]] ConnectionTls::Parent *connection,
    const std::error_code &ec) {
  parent_->error_code_ = ec;
  // Fill the backward compatible error retrieval.
  if (parent_->fill_request_by_callback_) {
    auto *holder = parent_->fill_request_by_callback_->holder_.get();
    holder->status_text = ec.message();
    holder->status = -1;
  }
}

void Client::CallbacksPrivateImpl::on_connection_close(
    ConnectionRaw::Parent *connection) {
  connection->get_socket().close();
  parent_->is_connected_ = false;
}

void Client::CallbacksPrivateImpl::on_connection_io_error(
    [[maybe_unused]] ConnectionRaw::Parent *connection,
    const std::error_code &ec) {
  parent_->error_code_ = ec;
  // Fill the backward compatible error retrieval.
  if (parent_->fill_request_by_callback_) {
    auto *holder = parent_->fill_request_by_callback_->holder_.get();
    holder->status_text = ec.message();
    holder->status = -1;
  }
}

}  // namespace client
}  // namespace http
