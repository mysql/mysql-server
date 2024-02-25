/*
  Copyright (c) 2018, 2023, Oracle and/or its affiliates.

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

#include <event2/bufferevent.h>
#include <event2/bufferevent_ssl.h>
#include <event2/event.h>
#include <event2/http.h>
#include <openssl/err.h>
#include <openssl/ssl.h>
#include <memory>

#include <iostream>
#include <stdexcept>
#include <string>

#include "http_request_impl.h"
#include "mysqlrouter/http_client.h"

class IOContext::impl {
 public:
  std::unique_ptr<event_base, std::function<void(event_base *)>> ev_base{
      nullptr, event_base_free};

  impl() { ev_base.reset(event_base_new()); }
};

IOContext::IOContext() : pImpl_{new impl()} {}

bool IOContext::dispatch() {
  int ret = event_base_dispatch(pImpl_->ev_base.get());

  if (ret == -1) {
    // we don't have an better error here
    throw std::runtime_error("event_base_dispath() error");
  }

  return ret == 0;
}

IOContext::~IOContext() = default;

HttpClient::~HttpClient() = default;

void HttpClient::make_request(HttpRequest *req, HttpMethod::type method,
                              const std::string &uri) {
  if (!conn_) conn_ = make_connection();
  conn_->make_request(req, method, uri);
}

void HttpClient::make_request_sync(HttpRequest *req, HttpMethod::type method,
                                   const std::string &uri) {
  if (!conn_) conn_ = make_connection();
  conn_->make_request_sync(req, method, uri);
}

HttpClient::operator bool() const {
  if (!conn_) return true;
  return conn_->operator bool();
}

std::string HttpClient::error_msg() const {
  if (!conn_) return {};
  return conn_->error_msg();
}

std::unique_ptr<HttpClientConnectionBase> HttpClient::make_connection() {
  return std::make_unique<HttpClientConnection>(io_ctx_, hostname_, port_);
}

std::unique_ptr<HttpClientConnectionBase> HttpsClient::make_connection() {
  return std::make_unique<HttpsClientConnection>(io_ctx_, tls_ctx_, hostname_,
                                                 port_);
}

// Client connections

HttpClientConnectionBase::~HttpClientConnectionBase() = default;

class HttpClientConnectionBase::impl {
 public:
  impl() = default;

  std::unique_ptr<evhttp_connection, decltype(&evhttp_connection_free)> conn{
      nullptr, &evhttp_connection_free};
};

event_base *HttpClientConnectionBase::ev_base() const {
  return io_ctx_.pImpl_->ev_base.get();
}

HttpClientConnectionBase::HttpClientConnectionBase(IOContext &io_ctx)
    : pImpl_{new impl()}, io_ctx_{io_ctx} {}

std::string HttpClientConnectionBase::error_msg() const {
  std::string out;
  if (pImpl_->conn) {
#ifdef EVENT__HAVE_OPENSSL
    auto *bev = evhttp_connection_get_bufferevent(pImpl_->conn.get());
    while (auto oslerr = bufferevent_get_openssl_error(bev)) {
      char buffer[256];

      ERR_error_string_n(oslerr, buffer, sizeof(buffer));

      out.append(buffer);
    }
#else
    out.append("SSL support disabled at compile-time");
#endif
  }
  return out;
}

HttpClientConnectionBase::operator bool() const {
  if (pImpl_->conn) {
#ifdef EVENT__HAVE_OPENSSL
    auto *bev = evhttp_connection_get_bufferevent(pImpl_->conn.get());
    SSL *ssl = bufferevent_openssl_get_ssl(bev);

    // if we have an SSL session, peek the error-queue
    if (nullptr != ssl) {
      return (ERR_peek_error() == 0) &&
             (SSL_get_verify_result(ssl) == X509_V_OK);
    }
#else
    return false;
#endif
  }
  return true;
}

// unencrypted HTTP

HttpClientConnection::HttpClientConnection(IOContext &io_ctx,
                                           const std::string &address,
                                           uint16_t port)
    : HttpClientConnectionBase{io_ctx} {
  bufferevent *bev =
      bufferevent_socket_new(ev_base(), -1, BEV_OPT_CLOSE_ON_FREE);
  pImpl_->conn.reset(evhttp_connection_base_bufferevent_new(
      ev_base(), nullptr, bev, address.c_str(), port));
}

void HttpClientConnectionBase::make_request(HttpRequest *req,
                                            HttpMethod::type method,
                                            const std::string &uri,
                                            std::chrono::seconds timeout) {
  if (!pImpl_->conn) {
    throw std::runtime_error("no connection set");
  }

  auto *ev_req = req->pImpl_->req.get();

  evhttp_connection_set_timeout(pImpl_->conn.get(), timeout.count());

  if (0 != evhttp_make_request(pImpl_->conn.get(), ev_req,
                               static_cast<enum evhttp_cmd_type>(method),
                               uri.c_str())) {
    throw std::runtime_error("evhttp_make_request() failed");
  }

  // don't free the evhttp_request() when HttpRequest gets destructed
  // as the eventloop will do it
  req->pImpl_->disown();
}

void HttpClientConnectionBase::make_request_sync(HttpRequest *req,
                                                 HttpMethod::type method,
                                                 const std::string &uri,
                                                 std::chrono::seconds timeout) {
  make_request(req, method, uri, timeout);

  io_ctx_.dispatch();
}

// encrypted HTTP

HttpsClientConnection::HttpsClientConnection(IOContext &io_ctx,
                                             TlsClientContext &tls_ctx,
                                             const std::string &address,
                                             uint16_t port)
    : HttpClientConnectionBase{io_ctx} {
#ifdef EVENT__HAVE_OPENSSL
  // owned by the bev
  SSL *ssl = SSL_new(tls_ctx.get());

  // enable SNI
  SSL_set_tlsext_host_name(ssl, const_cast<char *>(address.c_str()));

  // ownership moved to evhttp_connection_base_bufferevent_new
  bufferevent *bev = bufferevent_openssl_socket_new(
      ev_base(), -1, ssl, BUFFEREVENT_SSL_CONNECTING,
      BEV_OPT_CLOSE_ON_FREE | BEV_OPT_DEFER_CALLBACKS);

  // server will close socket before client could do a
  // SSL_shutdown(). libevent would treat at as fatal
  // error and throw away the request
  bufferevent_openssl_set_allow_dirty_shutdown(bev, 1);

  pImpl_->conn.reset(evhttp_connection_base_bufferevent_new(
      ev_base(), nullptr, bev, address.c_str(), port));
#else
  (void)io_ctx;
  (void)tls_ctx;
  (void)address;
  (void)port;
#endif
}
