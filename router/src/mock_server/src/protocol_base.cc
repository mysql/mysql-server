/*
  Copyright (c) 2020, Oracle and/or its affiliates. All rights reserved.

  This program is free software; you can redistribute it and/or modify
  it under the terms of the GNU General Public License, version 2.0,
  as published by the Free Software Foundation.

  This program is also distributed with certain software (including
  but not limited to OpenSSL) that is licensed under separate terms,
  as designated in a particular file or component or in included license
  documentation.  The authors of MySQL hereby grant you an additional
  permission to link the program and your derivative works with the
  separately licensed software that they have included with MySQL.

  This program is distributed in the hope that it will be useful,
  but WITHOUT ANY WARRANTY; without even the implied warranty of
  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
  GNU General Public License for more details.

  You should have received a copy of the GNU General Public License
  along with this program; if not, write to the Free Software
  Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA
*/

#include "statement_reader.h"

#include <string>
#include <vector>

#include "mysql/harness/tls_error.h"

namespace server_mock {
bool ProtocolBase::authenticate(const std::string &auth_method_name,
                                const std::string &auth_method_data,
                                const std::string &password,
                                const std::vector<uint8_t> &auth_response) {
  if (auth_method_name == CachingSha2Password::name) {
    auto scramble_res =
        CachingSha2Password::scramble(auth_method_data, password);
    return scramble_res && (scramble_res.value() == auth_response);
  } else if (auth_method_name == MySQLNativePassword::name) {
    auto scramble_res =
        MySQLNativePassword::scramble(auth_method_data, password);
    return scramble_res && (scramble_res.value() == auth_response);
  } else if (auth_method_name == ClearTextPassword::name) {
    auto scramble_res = ClearTextPassword::scramble(auth_method_data, password);
    return scramble_res && (scramble_res.value() == auth_response);
  } else {
    // there is also
    // - old_password (3.23, 4.0)
    // - sha256_password (5.6, ...)
    // - windows_authentication (5.6, ...)
    return false;
  }
}

stdx::expected<void, std::error_code> ProtocolBase::tls_accept() {
  auto ssl = ssl_.get();

  client_socket_.native_non_blocking(false);

  const auto res = SSL_accept(ssl);
  if (res != 1) {
    return stdx::make_unexpected(make_tls_ssl_error(ssl, res));
  }

  client_socket_.native_non_blocking(true);
  return {};
}
}  // namespace server_mock
