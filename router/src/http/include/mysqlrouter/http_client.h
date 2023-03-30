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

#ifndef MYSQLROUTER_HTTP_CLIENT_INCLUDED
#define MYSQLROUTER_HTTP_CLIENT_INCLUDED

#include <chrono>

#include "mysql/harness/tls_client_context.h"
#include "mysqlrouter/http_client_export.h"
#include "mysqlrouter/http_request.h"

struct evhttp_connection;
struct event_base;

/**
 * IO Context for network operations.
 *
 * wraps libevent's base
 */
class HTTP_CLIENT_EXPORT IOContext {
 public:
  IOContext();
  ~IOContext();

  /**
   * wait for events to fire and calls handlers.
   *
   * exits if no more pending events
   *
   * @returns false if no events were pending nor active, true otherwise
   * @throws  std::runtime_error on internal, unexpected error
   */
  bool dispatch();

 private:
  class impl;

  std::unique_ptr<impl> pImpl_;
  friend class HttpClientConnectionBase;
};

class HTTP_CLIENT_EXPORT HttpClientConnectionBase {
 public:
  ~HttpClientConnectionBase();

  void make_request(HttpRequest *req, HttpMethod::type method,
                    const std::string &uri,
                    std::chrono::seconds timeout = std::chrono::seconds{60});
  void make_request_sync(HttpRequest *req, HttpMethod::type method,
                         const std::string &uri,
                         std::chrono::seconds timeout = std::chrono::seconds{
                             60});

  /**
   * connection has an error.
   *
   * @see error_msg()
   */
  operator bool() const;

  /**
   * error-msg of the connection.
   *
   * @note may not be human friendly as it may come directly from openssl
   */
  std::string error_msg() const;

  /**
   * last socket errno.
   */
  std::error_code socket_errno() const { return socket_errno_; }

 protected:
  HttpClientConnectionBase(IOContext &io_ctx);

  class impl;

  std::unique_ptr<impl> pImpl_;

  IOContext &io_ctx_;

  std::error_code socket_errno_;

  /**
   * event-base associated with this connection.
   */
  event_base *ev_base() const;
};

class HTTP_CLIENT_EXPORT HttpClientConnection
    : public HttpClientConnectionBase {
 public:
  HttpClientConnection(IOContext &io_ctx, const std::string &address,
                       uint16_t port);
};

class HTTP_CLIENT_EXPORT HttpsClientConnection
    : public HttpClientConnectionBase {
 public:
  HttpsClientConnection(IOContext &io_ctx, TlsClientContext &tls_ctx,
                        const std::string &address, uint16_t port);
};

class HTTP_CLIENT_EXPORT HttpClient {
 public:
  HttpClient(IOContext &io_ctx, const std::string &hostname, uint16_t port)
      : io_ctx_{io_ctx}, hostname_{hostname}, port_{port} {}

  virtual ~HttpClient();

  /**
   * initiate a request on the bound IOContext.
   *
   * allows to send out multiple requests on different clients
   * and wait for them in parallel.
   */
  void make_request(HttpRequest *req, HttpMethod::type method,
                    const std::string &uri);

  /**
   * make a request and wait for the response.
   */
  void make_request_sync(HttpRequest *req, HttpMethod::type method,
                         const std::string &uri);

  /**
   * check if connection had an error.
   *
   * see: error_msg()
   */
  operator bool() const;

  /**
   * current error message.
   *
   * @see: HttpClientConnectionBase::error_msg()
   */
  std::string error_msg() const;

  /**
   * hostname to connect to.
   */
  std::string hostname() const { return hostname_; }

  /**
   * TCP port to connect to.
   */
  uint16_t port() const { return port_; }

 protected:
  virtual std::unique_ptr<HttpClientConnectionBase> make_connection();

  IOContext &io_ctx_;
  const std::string hostname_;
  uint16_t port_;

  std::unique_ptr<HttpClientConnectionBase> conn_;
};

class HTTP_CLIENT_EXPORT HttpsClient : public HttpClient {
 public:
  HttpsClient(IOContext &io_ctx, TlsClientContext &&tls_ctx,
              const std::string &address, uint16_t port)
      : HttpClient(io_ctx, address, port), tls_ctx_{std::move(tls_ctx)} {}

 private:
  std::unique_ptr<HttpClientConnectionBase> make_connection() override;

  TlsClientContext tls_ctx_;
};

#endif
