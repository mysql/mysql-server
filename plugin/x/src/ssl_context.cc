/*
 * Copyright (c) 2015, 2026, Oracle and/or its affiliates.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is designed to work with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have either included with
 * the program or referenced in the documentation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License, version 2.0, for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110-1301  USA
 */
#include <cinttypes>
#include <memory>
#include <string>

#include "violite.h"  // NOLINT(build/include_subdir)

#include "plugin/x/src/ngs/log.h"
#include "plugin/x/src/ssl_context.h"
#include "plugin/x/src/ssl_context_options.h"

namespace xpl {

static bool adjust_tls_flags_for_pqc(long *ssl_ctx_flags [[maybe_unused]],
                                     bool tls_force_pqc,
                                     enum_ssl_init_error *error) {
#if OPENSSL_VERSION_NUMBER >= 0x30500000L
  if (tls_force_pqc) {
    if (*ssl_ctx_flags & SSL_OP_NO_TLSv1_3) {
      *error = SSL_INITERR_PQC_UNSUPPORTED;
      return true;
    }

    *ssl_ctx_flags |= SSL_OP_NO_TLSv1_2;
  }
#else
  if (tls_force_pqc) {
    *error = SSL_INITERR_PQC_UNSUPPORTED;
    return true;
  }
#endif

  return false;
}

static void warn_tls_channel_without_force_pqc(bool force_pqc
                                               [[maybe_unused]]) {
#if OPENSSL_VERSION_NUMBER >= 0x30500000L
  if (!force_pqc) log_warning(ER_WARN_TLS_SESSION_WITHOUT_PQC, "mysqlx");
#endif
}

static const char *null_when_empty(const std::string &value) {
  return value.empty() ? nullptr : value.c_str();
}

Ssl_context::Ssl_context()
    : m_ssl_acceptor(nullptr), m_options(new Ssl_context_options()) {}

bool Ssl_context::setup(const iface::Ssl_context_config &config) {
  auto new_config = std::make_unique<iface::Ssl_context_config>(config);

  if (!setup(*new_config, true)) return false;

  m_config = std::move(new_config);
  return true;
}

bool Ssl_context::setup(const iface::Ssl_context_config &config,
                        bool warn_without_force_pqc) {
  enum_ssl_init_error error = SSL_INITERR_NOERROR;

  long ssl_ctx_flags = process_tls_version(null_when_empty(config.tls_version));
  const auto tls_force_pqc = config.tls_force_pqc;
  const auto tls_use_pqc_sign = config.tls_use_pqc_sign;
  const char *tls_kex = null_when_empty(config.tls_kex);

  if (adjust_tls_flags_for_pqc(&ssl_ctx_flags, tls_force_pqc, &error)) {
    log_warning(ER_XPLUGIN_FAILED_AT_SSL_CONF, sslGetErrString(error));
    return false;
  }

  auto *new_ssl_acceptor = new_VioSSLAcceptorFd(
      null_when_empty(config.ssl_key), null_when_empty(config.ssl_cert),
      null_when_empty(config.ssl_ca), null_when_empty(config.ssl_capath),
      null_when_empty(config.ssl_cipher), nullptr, &error,
      null_when_empty(config.ssl_crl), null_when_empty(config.ssl_crlpath),
      ssl_ctx_flags, tls_force_pqc, tls_use_pqc_sign, tls_kex);

  if (nullptr == new_ssl_acceptor) {
    log_warning(ER_XPLUGIN_FAILED_AT_SSL_CONF, sslGetErrString(error));
    return false;
  }

  auto new_options = std::make_unique<Ssl_context_options>(
      new_ssl_acceptor, config.tls_kex, tls_force_pqc, tls_use_pqc_sign);
  if (m_ssl_acceptor) free_vio_ssl_acceptor_fd(m_ssl_acceptor);
  m_ssl_acceptor = new_ssl_acceptor;
  m_options = std::move(new_options);
  if (warn_without_force_pqc) warn_tls_channel_without_force_pqc(tls_force_pqc);

  return true;
}

Ssl_context::~Ssl_context() {
  if (m_ssl_acceptor) free_vio_ssl_acceptor_fd(m_ssl_acceptor);
}

/** Start a TLS session in the connection.
 */
bool Ssl_context::activate_tls(iface::Vio *conn,
                               const int32_t handshake_timeout) {
  unsigned long error = 0;  // NOLINT(runtime/int)
  auto *vio = conn->get_vio();
  if (sslaccept(m_ssl_acceptor, vio, handshake_timeout, &error) != 0) {
    log_debug("Error during SSL handshake for client connection (%" PRIu64 ")",
              static_cast<uint64_t>(error));
    return false;
  }

  return true;
}

bool Ssl_context::reset() {
  if (!m_config) return true;
  return setup(*m_config, false);
}

}  // namespace xpl
