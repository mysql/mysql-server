/*
 * Copyright (c) 2015, 2018, Oracle and/or its affiliates. All rights reserved.
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

#ifndef PLUGIN_X_NGS_INCLUDE_NGS_INTERFACE_SSL_SESSION_OPTIONS_INTERFACE_H_
#define PLUGIN_X_NGS_INCLUDE_NGS_INTERFACE_SSL_SESSION_OPTIONS_INTERFACE_H_

#include <string>
#include <vector>

namespace ngs {

class Ssl_session_options_interface {
 public:
  virtual ~Ssl_session_options_interface() = default;

  virtual bool active_tls() const = 0;

  virtual std::string ssl_cipher() const = 0;
  virtual std::string ssl_version() const = 0;
  virtual std::vector<std::string> ssl_cipher_list() const = 0;

  virtual long ssl_verify_depth() const = 0;
  virtual long ssl_verify_mode() const = 0;
  virtual long ssl_sessions_reused() const = 0;

  virtual long ssl_get_verify_result_and_cert() const = 0;
  virtual std::string ssl_get_peer_certificate_issuer() const = 0;
  virtual std::string ssl_get_peer_certificate_subject() const = 0;
};

}  // namespace ngs

#endif  // PLUGIN_X_NGS_INCLUDE_NGS_INTERFACE_SSL_SESSION_OPTIONS_INTERFACE_H_
