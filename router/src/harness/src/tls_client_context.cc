/*
  Copyright (c) 2018, 2024, Oracle and/or its affiliates.

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

#include "mysql/harness/tls_client_context.h"

#include <mutex>

#include <openssl/ssl.h>

#include "mysql/harness/stdx/expected.h"
#include "mysql/harness/tls_error.h"
#include "openssl_version.h"

#include <dh_ecdh_config.h>

#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 1, 0)
#define TLS_CLIENT_METHOD() TLS_client_method()
#else
#define TLS_CLIENT_METHOD() SSLv23_client_method()
#endif

TlsClientContext::TlsClientContext(TlsVerify mode, bool session_cache_mode,
                                   size_t session_cache_size,
                                   std::chrono::seconds session_cache_timeout)
    : TlsContext(TLS_CLIENT_METHOD()),
      session_cache_mode_(session_cache_mode),
      session_cache_size_(session_cache_size),
      session_cache_timeout_(session_cache_timeout) {
  if (!ssl_ctx_.get()) return;

  (void)set_ecdh(ssl_ctx_.get());
  (void)set_dh(ssl_ctx_.get());
  verify(mode);

  const auto cache_mode =
      session_cache_mode_ ? SSL_SESS_CACHE_CLIENT : SSL_SESS_CACHE_OFF;
  SSL_CTX_set_session_cache_mode(ssl_ctx_.get(), cache_mode);
  if (session_cache_mode_) {
    sessions_ = std::make_unique<TlsClientContext::Sessions>();

    SSL_CTX_set_app_data(ssl_ctx_.get(), this);
    SSL_CTX_sess_set_new_cb(ssl_ctx_.get(),
                            [](SSL *ssl, SSL_SESSION *sess) -> int {
                              auto *self = reinterpret_cast<TlsClientContext *>(
                                  SSL_CTX_get_app_data(SSL_get_SSL_CTX(ssl)));
                              return self->add_session(sess).has_value();
                            });

    SSL_CTX_sess_set_remove_cb(ssl_ctx_.get(), [](SSL_CTX *ssl_ctx,
                                                  SSL_SESSION *sess) {
      auto *self =
          reinterpret_cast<TlsClientContext *>(SSL_CTX_get_app_data(ssl_ctx));
      self->remove_session(sess);
    });
  }
}

TlsClientContext::~TlsClientContext() {
  if (session_cache_mode_ && ssl_ctx_.get()) {
    SSL_CTX_sess_set_get_cb(ssl_ctx_.get(), nullptr);
    SSL_CTX_sess_set_remove_cb(ssl_ctx_.get(), nullptr);
  }
}

stdx::expected<void, std::error_code> TlsClientContext::verify(
    TlsVerify verify) {
  int mode = 0;
  switch (verify) {
    case TlsVerify::NONE:
      mode = SSL_VERIFY_NONE;
      break;
    case TlsVerify::PEER:
      mode = SSL_VERIFY_PEER;
      break;
  }

  SSL_CTX_set_verify(ssl_ctx_.get(), mode, nullptr);

  return {};
}

stdx::expected<void, std::error_code> TlsClientContext::cipher_suites(
    const std::string &ciphers) {
// TLSv1.3 ciphers are controlled via SSL_CTX_set_ciphersuites()
#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 1, 1)
  if (1 != SSL_CTX_set_ciphersuites(ssl_ctx_.get(), ciphers.c_str())) {
    return stdx::unexpected(make_tls_error());
  }
#else
  (void)ciphers;
  return stdx::unexpected(make_error_code(std::errc::function_not_supported));
#endif

  return {};
}

stdx::expected<void, std::error_code> TlsClientContext::cipher_list(
    const std::string &ciphers) {
  // TLSv1.3 ciphers are controlled via SSL_CTX_set_ciphersuites()
  if (1 != SSL_CTX_set_cipher_list(ssl_ctx_.get(), ciphers.c_str())) {
    return stdx::unexpected(make_tls_error());
  }

  return {};
}

stdx::expected<void, std::error_code> TlsClientContext::verify_hostname(
    const std::string &server_host) {
#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 0, 2)
  // get0_param() is added in openssl 1.0.2
  auto *ssl_ctx = ssl_ctx_.get();

  X509_VERIFY_PARAM *param = SSL_CTX_get0_param(ssl_ctx);
  /*
    As we don't know if the server_host contains IP addr or hostname
    call X509_VERIFY_PARAM_set1_ip_asc() first and if it returns an error
    (not valid IP address), call X509_VERIFY_PARAM_set1_host().
  */
  if (1 != X509_VERIFY_PARAM_set1_ip_asc(param, server_host.c_str())) {
    if (1 != X509_VERIFY_PARAM_set1_host(param, server_host.c_str(), 0)) {
      return stdx::unexpected(make_error_code(std::errc::invalid_argument));
    }
  }
#else
  (void)server_host;
  return stdx::unexpected(make_error_code(std::errc::function_not_supported));
#endif
  return {};
}

namespace {

const unsigned char *SSL_SESSION_get_id_wrapper(const SSL_SESSION *s,
                                                unsigned int *len) {
#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 1, 0)
  return SSL_SESSION_get_id(s, len);
#else
  if (len) *len = (unsigned int)s->session_id_length;
  return s->session_id;
#endif
}

int SSL_SESSION_is_resumable_wrapper(const SSL_SESSION *s) {
#if OPENSSL_VERSION_NUMBER >= ROUTER_OPENSSL_VERSION(1, 1, 1)
  return SSL_SESSION_is_resumable(s);
#else
  return !s->not_resumable;
#endif
}

}  // namespace

stdx::expected<void, std::error_code> TlsClientContext::add_session(
    SSL_SESSION *sess) {
  unsigned int id_len;
  const unsigned char *id = SSL_SESSION_get_id_wrapper(sess, &id_len);
  if (id_len == 0) return {};

  const Sessions::SessionId sess_id(id, id + id_len);

  std::lock_guard lk(sessions_->mtx_);
  auto &sessions = sessions_->sessions_;
  if (sessions.size() >= session_cache_size_) {
    sessions.pop_front();
  }
  sessions.emplace_back(sess_id, sess);

  return {};
}

stdx::expected<void, std::error_code> TlsClientContext::remove_session(
    SSL_SESSION *sess) {
  unsigned int id_len;
  const unsigned char *id = SSL_SESSION_get_id_wrapper(sess, &id_len);
  if (id_len == 0) return {};

  const Sessions::SessionId sess_id(id, id + id_len);

  std::lock_guard lk(sessions_->mtx_);
  auto &sessions = sessions_->sessions_;
  for (auto it = sessions.begin(); it != sessions.end(); ++it) {
    if (it->first == sess_id) {
      sessions.erase(it);
      return {};
    }
  }

  return {};
}

stdx::expected<SSL_SESSION *, std::error_code> TlsClientContext::get_session() {
  // sessions_ will be nullptr if caching is off
  if (sessions_) {
    std::lock_guard lk(sessions_->mtx_);
    auto &sessions = sessions_->sessions_;
    for (auto it = sessions.cbegin(); it != sessions.cend();) {
      const auto sess = it->second.get();
      const auto sess_start = SSL_SESSION_get_time(sess);
      if (time(nullptr) - sess_start > session_cache_timeout_.count()) {
        // session expired, remove from cache
        sessions.erase(it++);
        continue;
      }
      if (SSL_SESSION_is_resumable_wrapper(sess)) {
        return sess;
      }
      ++it;
    }
  }

  return stdx::unexpected(
      make_error_code(std::errc::no_such_file_or_directory));
}
