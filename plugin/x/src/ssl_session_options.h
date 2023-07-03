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

#ifndef PLUGIN_X_SRC_SSL_SESSION_OPTIONS_H_
#define PLUGIN_X_SRC_SSL_SESSION_OPTIONS_H_

#include <string>
#include <vector>

#include "plugin/x/src/interface/ssl_session_options.h"
#include "plugin/x/src/interface/vio.h"

namespace xpl {

class Ssl_session_options : public iface::Ssl_session_options {
 public:
  explicit Ssl_session_options(iface::Vio *vio) : m_vio(vio) {}

  bool active_tls() const override;
  std::string ssl_cipher() const override;
  std::string ssl_version() const override;
  std::vector<std::string> ssl_cipher_list() const override;

  int64_t ssl_verify_depth() const override;
  int64_t ssl_verify_mode() const override;

  int64_t ssl_sessions_reused() const override;
  int64_t ssl_get_verify_result_and_cert() const override;

  std::string ssl_get_peer_certificate_issuer() const override;
  std::string ssl_get_peer_certificate_subject() const override;

 private:
  iface::Vio *m_vio;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_SSL_SESSION_OPTIONS_H_
