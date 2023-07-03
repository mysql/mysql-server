/*
 * Copyright (c) 2017, 2022, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_INTERFACE_SSL_CONTEXT_H_
#define PLUGIN_X_SRC_INTERFACE_SSL_CONTEXT_H_

#include "plugin/x/src/interface/ssl_context_options.h"
#include "plugin/x/src/interface/vio.h"

namespace xpl {
namespace iface {

class Ssl_context {
 public:
  virtual ~Ssl_context() = default;

  virtual bool setup(const char *tls_version, const char *ssl_key,
                     const char *ssl_ca, const char *ssl_capath,
                     const char *ssl_cert, const char *ssl_cipher,
                     const char *ssl_crl, const char *ssl_crlpath) = 0;
  virtual bool activate_tls(Vio *conn, const int32_t handshake_timeout) = 0;

  virtual Ssl_context_options &options() = 0;
  virtual bool has_ssl() = 0;
  virtual void reset() = 0;
};

}  // namespace iface
}  // namespace xpl

#endif  // PLUGIN_X_SRC_INTERFACE_SSL_CONTEXT_H_
