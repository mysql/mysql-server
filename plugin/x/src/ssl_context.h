/*
 * Copyright (c) 2015, 2022, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_SSL_CONTEXT_H_
#define PLUGIN_X_SRC_SSL_CONTEXT_H_

#include <violite.h>
#include <memory>

#include "plugin/x/src/interface/ssl_context.h"
#include "plugin/x/src/interface/ssl_context_options.h"
#include "plugin/x/src/interface/vio.h"

namespace xpl {

class Ssl_context : public iface::Ssl_context {
 public:
  Ssl_context();
  bool setup(const char *tls_version, const char *ssl_key, const char *ssl_ca,
             const char *ssl_capath, const char *ssl_cert,
             const char *ssl_cipher, const char *ssl_crl,
             const char *ssl_crlpath) override;
  ~Ssl_context() override;

  bool activate_tls(iface::Vio *conn, const int32_t handshake_timeout) override;

  iface::Ssl_context_options &options() override { return *m_options; }
  bool has_ssl() override { return nullptr != m_ssl_acceptor; }
  void reset() override;

 private:
  struct Config;
  bool setup(const Config &config);

  st_VioSSLFd *m_ssl_acceptor;
  std::unique_ptr<iface::Ssl_context_options> m_options;
  std::unique_ptr<Config> m_config;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_SSL_CONTEXT_H_
