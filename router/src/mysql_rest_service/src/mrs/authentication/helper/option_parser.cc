/*
  Copyright (c) 2023, 2024, Oracle and/or its affiliates.

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

#include "mrs/authentication/helper/option_parser.h"

#include "mysql/harness/logging/logging.h"
#include "mysql/harness/string_utils.h"
#include "mysqlrouter/base64.h"

IMPORT_LOG_FUNCTIONS()

namespace mrs {
namespace authentication {

UserOptionsParser::UserOptionsParser(const std::string &auth_string)
    : auth_string_{auth_string} {}

bool UserOptionsParser::is_valid() const {
  if (auth_string_.empty()) {
    log_debug("UserOptionsParser, invalid input data.");
    return false;
  }

  if (!is_hmac_sha256) {
    log_debug("UserOptionsParser, invalid stored-key type.");
    return false;
  }

  if (!has_iterations_) {
    log_debug("UserOptionsParser, invalid number of iterations.");
    return false;
  }

  if (ksi.iterations < 5) {
    log_debug("UserOptionsParser, number of iterations too small.");
    return false;
  }

  return true;
}

UserOptionsParser::Result UserOptionsParser::decode() {
  parse();
  ksi.is_valid = is_valid();
  return ksi;
}

void UserOptionsParser::parse() {
  auto fields = mysql_harness::split_string(auth_string_, '$');
  if (5 != fields.size()) {
    log_debug("UserOptionsParser, invalid number of fields %i, expecting 5.",
              static_cast<int>(fields.size()));
    return;
  }

  try {
    is_hmac_sha256 = fields[1] == "A";
    ksi.iterations = strtoul(fields[2].c_str(), nullptr, 10);
    has_iterations_ = ksi.iterations > 4;
    ksi.salt = as_string(Base64::decode(fields[3]));
    ksi.stored_key = as_string(Base64::decode(fields[4]));
    ksi.iterations *= 1000;
  } catch (...) {
    auth_string_ = "";
  }
}

}  // namespace authentication
}  // namespace mrs
