/*
 * Copyright (c) 2015, 2024, Oracle and/or its affiliates.
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

#ifndef PLUGIN_X_SRC_AUTH_LEGACY_H_
#define PLUGIN_X_SRC_AUTH_LEGACY_H_

#include <memory>
#include <string>

#include "plugin/x/src/account_verification_handler.h"
#include "plugin/x/src/interface/authentication.h"
#include "plugin/x/src/interface/sha256_password_cache.h"

namespace xpl {

class Sasl_legacy_auth : public iface::Authentication {
 public:
  explicit Sasl_legacy_auth() {}

  static std::unique_ptr<iface::Authentication> create(
      iface::Session *session,
      iface::SHA256_password_cache *sha256_password_cache);

  Response handle_start(const std::string &mechanism, const std::string &data,
                        const std::string &initial_response) override;

  Response handle_continue(const std::string &data) override;

  ngs::Error_code authenticate_account(
      const std::string &user, const std::string &host,
      const std::string &passwd) const override;

  iface::Authentication_info get_authentication_info() const override {
    return m_auth_info;
  }

 private:
  iface::Authentication_info m_auth_info;
};

}  // namespace xpl

#endif  // PLUGIN_X_SRC_AUTH_LEGACY_H_
