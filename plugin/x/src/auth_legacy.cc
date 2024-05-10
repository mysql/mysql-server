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

#include <memory>

#include "plugin/x/src/auth_legacy.h"
#include "plugin/x/src/challenge_response_verification.h"

namespace xpl {

std::unique_ptr<iface::Authentication> Sasl_legacy_auth::create(
    [[maybe_unused]] iface::Session *session,
    [[maybe_unused]] iface::SHA256_password_cache *sha256_password_cache) {
  return std::make_unique<Sasl_legacy_auth>();
}

Sasl_legacy_auth::Response Sasl_legacy_auth::handle_start(const std::string &,
                                                          const std::string &,
                                                          const std::string &) {
  m_auth_info.reset();
  auto salt = Challenge_response_verification::generate_salt();

  return {Status::k_ongoing, 0, salt};
}

Sasl_legacy_auth::Response Sasl_legacy_auth::handle_continue(
    const std::string &sasl_message) {
  std::string schema;
  std::string account;
  std::string passwd;

  // Ignore parsing errors. Old code sent access-denind also when prase failed.
  Account_verification_handler::parse_sasl_message(sasl_message, &m_auth_info,
                                                   &schema, &account, &passwd);

  auto access_denied = ngs::SQLError_access_denied();

  return {Status::k_failed, access_denied.error, access_denied.message};
}

ngs::Error_code Sasl_legacy_auth::authenticate_account(
    [[maybe_unused]] const std::string &user,
    [[maybe_unused]] const std::string &host,
    [[maybe_unused]] const std::string &passwd) const {
  return ngs::SQLError_access_denied();
}

}  // namespace xpl
