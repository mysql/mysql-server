/*
 * Copyright (c) 2015, 2023, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_INTERFACE_SSL_SESSION_OPTIONS_H_
#define PLUGIN_X_SRC_INTERFACE_SSL_SESSION_OPTIONS_H_

#include <string>
#include <vector>

namespace xpl {
namespace iface {

class Ssl_session_options {
 public:
  virtual ~Ssl_session_options() = default;

  virtual bool active_tls() const = 0;

  virtual std::string ssl_cipher() const = 0;
  virtual std::string ssl_version() const = 0;
  virtual std::vector<std::string> ssl_cipher_list() const = 0;

  virtual int64_t ssl_verify_depth() const = 0;
  virtual int64_t ssl_verify_mode() const = 0;
  virtual int64_t ssl_sessions_reused() const = 0;

  virtual int64_t ssl_get_verify_result_and_cert() const = 0;
  virtual std::string ssl_get_peer_certificate_issuer() const = 0;
  virtual std::string ssl_get_peer_certificate_subject() const = 0;
};

}  // namespace iface
}  // namespace xpl

#endif  // PLUGIN_X_SRC_INTERFACE_SSL_SESSION_OPTIONS_H_
