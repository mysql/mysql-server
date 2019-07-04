/*
 * Copyright (c) 2015, 2019, Oracle and/or its affiliates. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License, version 2.0,
 * as published by the Free Software Foundation.
 *
 * This program is also distributed with certain software (including
 * but not limited to OpenSSL) that is licensed under separate terms,
 * as designated in a particular file or component or in included license
 * documentation.  The authors of MySQL hereby grant you an additional
 * permission to link the program and your derivative works with the
 * separately licensed software that they have included with MySQL.
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

#include "violite.h"

#include "plugin/x/ngs/include/ngs/log.h"
#include "plugin/x/src/ssl_context.h"
#include "plugin/x/src/ssl_context_options.h"

namespace xpl {

struct Ssl_context::Config {
  class Value {
   public:
    Value(const char *value) : m_value(value ? value : "") {}
    operator const char *() const {
      return m_value.empty() ? nullptr : m_value.c_str();
    }

   private:
    std::string m_value;
  };
  Value tls_version;
  Value ssl_key;
  Value ssl_ca;
  Value ssl_capath;
  Value ssl_cert;
  Value ssl_cipher;
  Value ssl_crl;
  Value ssl_crlpath;
};

Ssl_context::Ssl_context()
    : m_ssl_acceptor(nullptr), m_options(new Ssl_context_options()) {}

bool Ssl_context::setup(const char *tls_version, const char *ssl_key,
                        const char *ssl_ca, const char *ssl_capath,
                        const char *ssl_cert, const char *ssl_cipher,
                        const char *ssl_crl, const char *ssl_crlpath) {
  m_config.reset(new Config{tls_version, ssl_key, ssl_ca, ssl_capath, ssl_cert,
                            ssl_cipher, ssl_crl, ssl_crlpath});
  return setup(*m_config);
}

bool Ssl_context::setup(const Config &config) {
  enum_ssl_init_error error = SSL_INITERR_NOERROR;

  long ssl_ctx_flags = process_tls_version(config.tls_version);

  m_ssl_acceptor =
      new_VioSSLAcceptorFd(config.ssl_key, config.ssl_cert, config.ssl_ca,
                           config.ssl_capath, config.ssl_cipher, NULL, &error,
                           config.ssl_crl, config.ssl_crlpath, ssl_ctx_flags);

  if (NULL == m_ssl_acceptor) {
    log_warning(ER_XPLUGIN_FAILED_AT_SSL_CONF, sslGetErrString(error));
    return false;
  }

  m_options.reset(new Ssl_context_options(m_ssl_acceptor));

  return true;
}

Ssl_context::~Ssl_context() {
  if (m_ssl_acceptor) free_vio_ssl_acceptor_fd(m_ssl_acceptor);
}

/** Start a TLS session in the connection.
 */
bool Ssl_context::activate_tls(ngs::Vio_interface *conn,
                               const int handshake_timeout) {
  unsigned long error;
  auto vio = conn->get_vio();
  if (sslaccept(m_ssl_acceptor, vio, handshake_timeout, &error) != 0) {
    log_debug("Error during SSL handshake for client connection (%i)",
              (int)error);
    return false;
  }

  return true;
}

void Ssl_context::reset() {
  if (!m_config || !m_ssl_acceptor) return;
  free_vio_ssl_acceptor_fd(m_ssl_acceptor);
  setup(*m_config);
}

}  // namespace xpl
